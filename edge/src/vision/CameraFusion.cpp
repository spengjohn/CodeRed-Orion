#include "CameraFusion.hpp"

#include <algorithm>
#include <cstring>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "util/stb_image_write.h"

namespace {

void ensure_frame(RgbFrame& out) {
    out.width = CameraFusion::out_width;
    out.height = CameraFusion::out_height;
    out.rgb.resize(static_cast<std::size_t>(
        CameraFusion::out_width * CameraFusion::out_height * 3
    ));
}

void sample_ir_rgb(
    const OV5647& camera,
    float x,
    float y,
    unsigned* out_r,
    unsigned* out_g,
    unsigned* out_b
) {
    const int w = OV5647::width;
    const int h = OV5647::height;
    x = std::clamp(x, 0.0f, static_cast<float>(w - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(h - 1));

    const int x0 = static_cast<int>(x);
    const int y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const float ifx = 1.0f - fx;
    const float ify = 1.0f - fy;

    const int i00 = y0 * w + x0;
    const int i10 = y0 * w + x1;
    const int i01 = y1 * w + x0;
    const int i11 = y1 * w + x1;

    const float r =
        (static_cast<float>(camera.r[i00]) * ifx
            + static_cast<float>(camera.r[i10]) * fx) * ify
        + (static_cast<float>(camera.r[i01]) * ifx
            + static_cast<float>(camera.r[i11]) * fx) * fy;
    const float g =
        (static_cast<float>(camera.g[i00]) * ifx
            + static_cast<float>(camera.g[i10]) * fx) * ify
        + (static_cast<float>(camera.g[i01]) * ifx
            + static_cast<float>(camera.g[i11]) * fx) * fy;
    const float b =
        (static_cast<float>(camera.b[i00]) * ifx
            + static_cast<float>(camera.b[i10]) * fx) * ify
        + (static_cast<float>(camera.b[i01]) * ifx
            + static_cast<float>(camera.b[i11]) * fx) * fy;

    *out_r = static_cast<unsigned>(r + 0.5f);
    *out_g = static_cast<unsigned>(g + 0.5f);
    *out_b = static_cast<unsigned>(b + 0.5f);
}

struct JpegBuf {
    std::vector<std::uint8_t>* out;
};

void jpeg_write_cb(void* context, void* data, int size) {
    auto* buf = static_cast<JpegBuf*>(context);
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    buf->out->insert(buf->out->end(), bytes, bytes + size);
}

}

bool CameraFusion::init() {
    return true;
}

void CameraFusion::shutdown() {}

bool CameraFusion::render_ir(const OV5647& camera, RgbFrame& out) {
    if (!camera.ready) {
        return false;
    }

    ensure_frame(out);

    const float ir_w = static_cast<float>(OV5647::width);
    const float ir_h = static_cast<float>(OV5647::height);

    for (int y = 0; y < out_height; ++y) {
        const float v = (static_cast<float>(y) + 0.5f)
            / static_cast<float>(out_height);
        const float ir_sy = v * ir_h - 0.5f;

        for (int x = 0; x < out_width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f)
                / static_cast<float>(out_width);
            const float ir_sx = u * ir_w - 0.5f;

            unsigned r = 0;
            unsigned g = 0;
            unsigned b = 0;
            sample_ir_rgb(camera, ir_sx, ir_sy, &r, &g, &b);

            const std::size_t i = static_cast<std::size_t>((y * out_width + x) * 3);
            out.rgb[i + 0] = static_cast<std::uint8_t>(r);
            out.rgb[i + 1] = static_cast<std::uint8_t>(g);
            out.rgb[i + 2] = static_cast<std::uint8_t>(b);
        }
    }
    return true;
}

bool CameraFusion::render_fusion(
    const OV5647& camera,
    const MLX90640Driver& thermal,
    RgbFrame& out
) {
    (void)thermal;
    return render_ir(camera, out);
}

bool CameraFusion::encode_jpeg(
    const RgbFrame& frame,
    std::vector<std::uint8_t>& jpeg_out,
    int quality
) {
    if (frame.width <= 0 || frame.height <= 0 || frame.rgb.empty()) {
        return false;
    }
    quality = std::clamp(quality, 1, 100);
    jpeg_out.clear();
    JpegBuf buf{&jpeg_out};
    const int ok = stbi_write_jpg_to_func(
        jpeg_write_cb,
        &buf,
        frame.width,
        frame.height,
        3,
        frame.rgb.data(),
        quality
    );
    if (!ok || jpeg_out.empty()) {
        jpeg_out.clear();
        return false;
    }
    return true;
}
