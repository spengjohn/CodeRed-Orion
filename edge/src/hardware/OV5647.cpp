#include "OV5647.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(__linux__) && defined(HAS_LIBCAMERA)
#include <libcamera/libcamera.h>
#include <libcamera/logging.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr unsigned kCaptureWidth = 640;
constexpr unsigned kCaptureHeight = 480;
constexpr int kFirstFrameWaitMs = 8000;
constexpr int kFrameWaitMs = 40;
constexpr int kStallFramesBeforeDemo = 75;

uint8_t clamp_u8(int value) {
    return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

uint8_t luma(uint8_t rr, uint8_t gg, uint8_t bb) {
    return static_cast<uint8_t>((77u * rr + 150u * gg + 29u * bb) >> 8);
}

void yuv_bt601_to_rgb(uint8_t y, uint8_t u, uint8_t v, uint8_t* rr, uint8_t* gg, uint8_t* bb) {
    const int c = static_cast<int>(y) - 16;
    const int d = static_cast<int>(u) - 128;
    const int e = static_cast<int>(v) - 128;
    *rr = clamp_u8((298 * c + 409 * e + 128) >> 8);
    *gg = clamp_u8((298 * c - 100 * d - 208 * e + 128) >> 8);
    *bb = clamp_u8((298 * c + 516 * d + 128) >> 8);
}

void fill_demo_frame(OV5647& cam) {
    const float t = static_cast<float>(cam.frame_index);
    const float cx = (OV5647::width - 1) * 0.5f + std::sin(t * 0.05f) * 55.0f;
    const float cy = (OV5647::height - 1) * 0.5f + std::cos(t * 0.04f) * 35.0f;
    const float sigma = 22.0f;
    cam.v_min = 255;
    cam.v_max = 0;

    for (int y = 0; y < OV5647::height; ++y) {
        for (int x = 0; x < OV5647::width; ++x) {
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float blob = std::exp(-(dx * dx + dy * dy) / (2.0f * sigma * sigma));
            const float wash = 0.08f * std::sin(0.04f * static_cast<float>(x + y) + t * 0.03f);
            const float level = std::clamp(0.12f + 0.78f * blob + wash, 0.0f, 1.0f);
            const uint8_t gray = clamp_u8(static_cast<int>(level * 255.0f + 0.5f));
            const uint8_t tint = clamp_u8(static_cast<int>(gray * 0.85f + 20.0f));
            const int i = y * OV5647::width + x;
            cam.r[i] = tint;
            cam.g[i] = gray;
            cam.b[i] = clamp_u8(static_cast<int>(gray * 0.70f + 10.0f));
            const uint8_t lum = luma(cam.r[i], cam.g[i], cam.b[i]);
            cam.v_min = std::min(cam.v_min, lum);
            cam.v_max = std::max(cam.v_max, lum);
        }
    }

    ++cam.frame_index;
}

#if defined(__linux__) && defined(HAS_LIBCAMERA)

using namespace libcamera;

struct MappedPlane {
    void* base = nullptr;
    size_t size = 0;
    const uint8_t* data = nullptr;
};

struct LatestFrame {
    uint8_t r[OV5647::pixel_count]{};
    uint8_t g[OV5647::pixel_count]{};
    uint8_t b[OV5647::pixel_count]{};
    uint8_t vmin = 0;
    uint8_t vmax = 255;
    int index = 0;
    bool valid = false;
};

struct LiveCapture {
    std::unique_ptr<CameraManager> manager;
    std::shared_ptr<Camera> camera;
    std::unique_ptr<CameraConfiguration> config;
    std::unique_ptr<FrameBufferAllocator> allocator;
    Stream* stream = nullptr;
    std::vector<std::unique_ptr<Request>> requests;
    std::unordered_map<FrameBuffer*, MappedPlane> planes;
    PixelFormat format{};
    unsigned stride = 0;
    unsigned width = kCaptureWidth;
    unsigned height = kCaptureHeight;
    bool active = false;
    int miss_count = 0;
    std::string fail;
    std::mutex lifecycle_mu;
    std::mutex frame_mu;
    std::condition_variable frame_cv;
    LatestFrame latest{};
    uint8_t scratch_r[OV5647::pixel_count]{};
    uint8_t scratch_g[OV5647::pixel_count]{};
    uint8_t scratch_b[OV5647::pixel_count]{};
};

LiveCapture g_live;

void set_fail(const char* reason) {
    g_live.fail = reason ? reason : "unknown capture error";
}

void copy_fail(OV5647& cam) {
    std::snprintf(
        cam.last_error,
        sizeof(cam.last_error),
        "%s",
        g_live.fail.empty() ? "ov5647 live path failed" : g_live.fail.c_str()
    );
}

void quiet_libcamera() {
    setenv("LIBCAMERA_LOG_LEVELS", "*:FATAL", 1);
    logSetTarget(LoggingTargetNone);
}

bool id_is_ov5647(const std::string& id) {
    std::string lower = id;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.find("ov5647") != std::string::npos;
}

std::shared_ptr<Camera> select_camera(CameraManager& manager) {
    const auto cameras = manager.cameras();
    if (cameras.empty()) {
        return nullptr;
    }
    for (const auto& cam : cameras) {
        if (id_is_ov5647(cam->id())) {
            return manager.get(cam->id());
        }
    }
    return manager.get(cameras.front()->id());
}

void sample_pixel(
    const uint8_t* src,
    unsigned sx,
    unsigned sy,
    uint8_t* out_r,
    uint8_t* out_g,
    uint8_t* out_b
) {
    const size_t row = static_cast<size_t>(sy) * g_live.stride;

    if (g_live.format == formats::NV12 || g_live.format == formats::NV21) {
        const uint8_t y = src[row + sx];
        const uint8_t* uv = src + static_cast<size_t>(g_live.stride) * g_live.height;
        const size_t uv_i = static_cast<size_t>(sy / 2) * g_live.stride
            + static_cast<size_t>(sx & ~1u);
        const uint8_t u = (g_live.format == formats::NV12) ? uv[uv_i] : uv[uv_i + 1];
        const uint8_t v = (g_live.format == formats::NV12) ? uv[uv_i + 1] : uv[uv_i];
        yuv_bt601_to_rgb(y, u, v, out_r, out_g, out_b);
        return;
    }

    if (g_live.format == formats::YUV420 || g_live.format == formats::YVU420) {
        const uint8_t y = src[row + sx];
        const size_t y_bytes = static_cast<size_t>(g_live.stride) * g_live.height;
        const size_t chroma_stride = g_live.stride / 2;
        const size_t chroma_h = (g_live.height + 1) / 2;
        const size_t plane_a = y_bytes;
        const size_t plane_b = y_bytes + chroma_stride * chroma_h;
        const size_t ci = static_cast<size_t>(sy / 2) * chroma_stride
            + static_cast<size_t>(sx / 2);
        const uint8_t u = (g_live.format == formats::YUV420)
            ? src[plane_a + ci]
            : src[plane_b + ci];
        const uint8_t v = (g_live.format == formats::YUV420)
            ? src[plane_b + ci]
            : src[plane_a + ci];
        yuv_bt601_to_rgb(y, u, v, out_r, out_g, out_b);
        return;
    }

    if (g_live.format == formats::YUYV) {
        const uint8_t* px = src + row + static_cast<size_t>(sx) * 2;
        const uint8_t* pair = src + row + static_cast<size_t>(sx & ~1u) * 2;
        yuv_bt601_to_rgb(px[0], pair[1], pair[3], out_r, out_g, out_b);
        return;
    }

    unsigned bpp = 4;
    unsigned r_off = 0;
    unsigned g_off = 1;
    unsigned b_off = 2;
    if (g_live.format == formats::RGB888) {
        bpp = 3;
    } else if (g_live.format == formats::BGR888) {
        bpp = 3;
        r_off = 2;
        b_off = 0;
    } else if (g_live.format == formats::XRGB8888 || g_live.format == formats::ARGB8888) {
        r_off = 2;
        b_off = 0;
    } else if (
        g_live.format != formats::XBGR8888
        && g_live.format != formats::ABGR8888
    ) {
        const uint8_t y = src[row + sx];
        *out_r = y;
        *out_g = y;
        *out_b = y;
        return;
    }

    const uint8_t* px = src + row + static_cast<size_t>(sx) * bpp;
    *out_r = px[r_off];
    *out_g = px[g_off];
    *out_b = px[b_off];
}

void downsample_frame(
    const uint8_t* src,
    uint8_t* out_r,
    uint8_t* out_g,
    uint8_t* out_b,
    uint8_t* out_min,
    uint8_t* out_max
) {
    *out_min = 255;
    *out_max = 0;
    for (int y = 0; y < OV5647::height; ++y) {
        const unsigned sy = static_cast<unsigned>(
            (static_cast<uint64_t>(y) * g_live.height)
            / static_cast<unsigned>(OV5647::height)
        );
        for (int x = 0; x < OV5647::width; ++x) {
            const unsigned sx = static_cast<unsigned>(
                (static_cast<uint64_t>(x) * g_live.width)
                / static_cast<unsigned>(OV5647::width)
            );
            const int i = y * OV5647::width + x;
            sample_pixel(src, sx, sy, &out_r[i], &out_g[i], &out_b[i]);
            const uint8_t lum = luma(out_r[i], out_g[i], out_b[i]);
            *out_min = std::min(*out_min, lum);
            *out_max = std::max(*out_max, lum);
        }
    }
}

void on_request_complete(Request* request);

void teardown_live() {
    {
        std::lock_guard<std::mutex> lock(g_live.lifecycle_mu);
        g_live.active = false;
        if (g_live.camera) {
            g_live.camera->requestCompleted.disconnect(on_request_complete);
        }
    }

    if (g_live.camera) {
        g_live.camera->stop();
    }

    {
        std::lock_guard<std::mutex> lock(g_live.frame_mu);
        g_live.latest.valid = false;
    }

    for (auto& entry : g_live.planes) {
        if (entry.second.base != nullptr) {
            munmap(entry.second.base, entry.second.size);
        }
    }
    g_live.planes.clear();
    g_live.requests.clear();

    if (g_live.allocator && g_live.stream) {
        g_live.allocator->free(g_live.stream);
    }
    g_live.allocator.reset();
    g_live.config.reset();
    g_live.stream = nullptr;

    if (g_live.camera) {
        g_live.camera->release();
        g_live.camera.reset();
    }
    if (g_live.manager) {
        g_live.manager->stop();
        g_live.manager.reset();
    }
    g_live.miss_count = 0;
}

void on_request_complete(Request* request) {
    Camera* camera = nullptr;
    {
        std::lock_guard<std::mutex> life(g_live.lifecycle_mu);
        if (
            request == nullptr
            || request->status() == Request::RequestCancelled
            || !g_live.active
            || !g_live.camera
        ) {
            return;
        }
        camera = g_live.camera.get();

        for (const auto& buffer_pair : request->buffers()) {
            FrameBuffer* buffer = buffer_pair.second;
            if (buffer->metadata().status != FrameMetadata::FrameSuccess) {
                continue;
            }
            const auto it = g_live.planes.find(buffer);
            if (it == g_live.planes.end() || it->second.data == nullptr) {
                continue;
            }

            uint8_t vmin = 0;
            uint8_t vmax = 255;
            downsample_frame(
                it->second.data,
                g_live.scratch_r,
                g_live.scratch_g,
                g_live.scratch_b,
                &vmin,
                &vmax
            );

            {
                std::lock_guard<std::mutex> frame(g_live.frame_mu);
                std::memcpy(g_live.latest.r, g_live.scratch_r, sizeof(g_live.latest.r));
                std::memcpy(g_live.latest.g, g_live.scratch_g, sizeof(g_live.latest.g));
                std::memcpy(g_live.latest.b, g_live.scratch_b, sizeof(g_live.latest.b));
                g_live.latest.vmin = vmin;
                g_live.latest.vmax = vmax;
                ++g_live.latest.index;
                g_live.latest.valid = true;
            }
            g_live.frame_cv.notify_one();
        }

        request->reuse(Request::ReuseBuffers);
    }

    if (camera != nullptr) {
        camera->queueRequest(request);
    }
}

bool map_framebuffers(const std::vector<std::unique_ptr<FrameBuffer>>& buffers) {
    for (const auto& buffer : buffers) {
        const auto& planes = buffer->planes();
        if (planes.empty()) {
            set_fail("empty framebuffer planes");
            return false;
        }

        const int fd = planes[0].fd.get();
        size_t map_size = 0;
        for (const auto& plane : planes) {
            if (plane.fd.get() != fd) {
                continue;
            }
            map_size = std::max(
                map_size,
                static_cast<size_t>(plane.offset) + plane.length
            );
        }
        if (map_size == 0) {
            set_fail("zero-sized framebuffer map");
            return false;
        }

        void* memory = mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (memory == MAP_FAILED) {
            set_fail("framebuffer mmap failed");
            return false;
        }

        MappedPlane mapped;
        mapped.base = memory;
        mapped.size = map_size;
        mapped.data = static_cast<const uint8_t*>(memory) + planes[0].offset;
        g_live.planes[buffer.get()] = mapped;
    }
    return true;
}

bool configure_stream() {
    g_live.config = g_live.camera->generateConfiguration({ StreamRole::Viewfinder });
    if (!g_live.config || g_live.config->empty()) {
        set_fail("viewfinder config unavailable");
        return false;
    }

    auto& stream_cfg = g_live.config->at(0);
    stream_cfg.size.width = kCaptureWidth;
    stream_cfg.size.height = kCaptureHeight;
    stream_cfg.pixelFormat = formats::RGB888;

    if (g_live.config->validate() == CameraConfiguration::Invalid) {
        g_live.config = g_live.camera->generateConfiguration({ StreamRole::Viewfinder });
        if (!g_live.config || g_live.config->empty()) {
            set_fail("viewfinder retry failed");
            return false;
        }
        auto& retry = g_live.config->at(0);
        retry.size.width = kCaptureWidth;
        retry.size.height = kCaptureHeight;
        retry.pixelFormat = formats::XBGR8888;
        if (g_live.config->validate() == CameraConfiguration::Invalid) {
            g_live.config = g_live.camera->generateConfiguration({ StreamRole::Viewfinder });
            if (
                !g_live.config
                || g_live.config->empty()
                || g_live.config->validate() == CameraConfiguration::Invalid
            ) {
                set_fail("no valid viewfinder mode");
                return false;
            }
        }
    }

    if (g_live.camera->configure(g_live.config.get()) < 0) {
        set_fail("camera configure rejected");
        return false;
    }

    auto& applied = g_live.config->at(0);
    g_live.format = applied.pixelFormat;
    g_live.stride = applied.stride;
    g_live.width = applied.size.width;
    g_live.height = applied.size.height;
    g_live.stream = applied.stream();
    return true;
}

bool open_live() {
    teardown_live();
    g_live.fail.clear();
    quiet_libcamera();

    g_live.manager = std::make_unique<CameraManager>();
    if (g_live.manager->start() != 0) {
        set_fail("camera manager start failed");
        teardown_live();
        return false;
    }
    if (g_live.manager->cameras().empty()) {
        set_fail("no csi cameras detected");
        teardown_live();
        return false;
    }

    g_live.camera = select_camera(*g_live.manager);
    if (!g_live.camera) {
        set_fail("unable to open csi camera");
        teardown_live();
        return false;
    }
    if (g_live.camera->acquire() != 0) {
        set_fail("camera busy / acquire failed");
        teardown_live();
        return false;
    }
    if (!configure_stream()) {
        teardown_live();
        return false;
    }

    g_live.allocator = std::make_unique<FrameBufferAllocator>(g_live.camera);
    if (g_live.allocator->allocate(g_live.stream) < 0) {
        set_fail("framebuffer allocate failed");
        teardown_live();
        return false;
    }

    const auto& buffers = g_live.allocator->buffers(g_live.stream);
    if (buffers.empty()) {
        set_fail("allocator returned no buffers");
        teardown_live();
        return false;
    }
    if (!map_framebuffers(buffers)) {
        teardown_live();
        return false;
    }

    for (const auto& buffer : buffers) {
        auto request = g_live.camera->createRequest();
        if (!request || request->addBuffer(g_live.stream, buffer.get()) < 0) {
            set_fail("request buffer setup failed");
            teardown_live();
            return false;
        }
        g_live.requests.push_back(std::move(request));
    }

    g_live.camera->requestCompleted.connect(on_request_complete);
    {
        std::lock_guard<std::mutex> lock(g_live.frame_mu);
        g_live.latest.valid = false;
        g_live.latest.index = 0;
    }
    {
        std::lock_guard<std::mutex> lock(g_live.lifecycle_mu);
        g_live.active = true;
    }

    if (g_live.camera->start() < 0) {
        set_fail("camera start failed");
        teardown_live();
        return false;
    }
    for (auto& request : g_live.requests) {
        if (g_live.camera->queueRequest(request.get()) < 0) {
            set_fail("initial queueRequest failed");
            teardown_live();
            return false;
        }
    }

    {
        std::unique_lock<std::mutex> lock(g_live.frame_mu);
        g_live.frame_cv.wait_for(
            lock,
            std::chrono::milliseconds(kFirstFrameWaitMs),
            [] { return g_live.latest.valid; }
        );
        if (!g_live.latest.valid) {
            lock.unlock();
            set_fail("first frame timeout");
            teardown_live();
            return false;
        }
    }

    g_live.fail.clear();
    return true;
}

bool pull_live_frame(OV5647& cam) {
    if (!g_live.active) {
        return false;
    }

    std::unique_lock<std::mutex> lock(g_live.frame_mu);
    if (!g_live.latest.valid) {
        g_live.frame_cv.wait_for(
            lock,
            std::chrono::milliseconds(kFrameWaitMs),
            [] { return g_live.latest.valid; }
        );
    }
    if (!g_live.latest.valid) {
        return false;
    }

    std::memcpy(cam.r, g_live.latest.r, sizeof(cam.r));
    std::memcpy(cam.g, g_live.latest.g, sizeof(cam.g));
    std::memcpy(cam.b, g_live.latest.b, sizeof(cam.b));
    cam.v_min = g_live.latest.vmin;
    cam.v_max = g_live.latest.vmax;
    cam.frame_index = g_live.latest.index;
    return true;
}

#endif

} // namespace

bool OV5647::init() {
    frame_index = 0;
    last_error[0] = '\0';

#if defined(__linux__) && defined(HAS_LIBCAMERA)
    if (open_live()) {
        demo_mode = false;
        ready = true;
        last_error[0] = '\0';
        return refresh();
    }
    copy_fail(*this);
#else
    std::snprintf(last_error, sizeof(last_error), "libcamera disabled at build time");
#endif

    demo_mode = true;
    ready = true;
    fill_demo_frame(*this);
    return true;
}

bool OV5647::refresh() {
    if (!ready) {
        return false;
    }

#if defined(__linux__) && defined(HAS_LIBCAMERA)
    if (!demo_mode) {
        if (pull_live_frame(*this)) {
            g_live.miss_count = 0;
            return true;
        }
        ++g_live.miss_count;
        if (g_live.miss_count < kStallFramesBeforeDemo) {
            return true;
        }
        set_fail("live stream stalled");
        copy_fail(*this);
        demo_mode = true;
        teardown_live();
    }
#endif

    fill_demo_frame(*this);
    return true;
}

void OV5647::shutdown() {
    ready = false;
    demo_mode = true;
    frame_index = 0;
    last_error[0] = '\0';

#if defined(__linux__) && defined(HAS_LIBCAMERA)
    teardown_live();
    g_live.fail.clear();
#endif
}

bool OV5647::pack_rgb888(uint8_t* rgb_out) const {
    if (rgb_out == nullptr) {
        return false;
    }
    for (int i = 0; i < pixel_count; ++i) {
        rgb_out[i * 3 + 0] = r[i];
        rgb_out[i * 3 + 1] = g[i];
        rgb_out[i * 3 + 2] = b[i];
    }
    return true;
}
