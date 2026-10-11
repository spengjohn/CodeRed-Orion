#include "IrStreamPipeline.hpp"

#include <iostream>

bool IrStreamPipeline::init(
    const Config& config,
    OV5647& cam,
    CameraFusion& fus,
    UplinkClient& client,
    MLX90640Driver* therm
) {
    stop_worker();
    cfg = config;
    camera = &cam;
    fusion = &fus;
    thermal = therm;
    uplink = &client;
    frames_posted = 0;
    frames_failed = 0;
    ready = camera != nullptr && fusion != nullptr && uplink != nullptr;
    if (ready) {
        start_worker();
    }
    return ready;
}

bool IrStreamPipeline::process_frame() {
    if (!ready || camera == nullptr || !camera->ready) {
        return false;
    }
    if (!render_frame()) {
        return false;
    }
    if (!encode_frame()) {
        return false;
    }
    return upload_frame();
}

void IrStreamPipeline::shutdown() {
    stop_worker();
    ready = false;
    camera = nullptr;
    fusion = nullptr;
    thermal = nullptr;
    uplink = nullptr;
    jpeg.clear();
    frame.rgb.clear();
}

bool IrStreamPipeline::render_frame() {
    if (fusion == nullptr || camera == nullptr) {
        return false;
    }
    if (thermal != nullptr && thermal->ready) {
        if (fusion->render_fusion(*camera, *thermal, frame)) {
            return true;
        }
    }
    return fusion->render_ir(*camera, frame);
}

bool IrStreamPipeline::encode_frame() {
    if (fusion == nullptr) {
        return false;
    }
    const int quality = cfg.jpeg_quality > 0 ? cfg.jpeg_quality : 70;
    return fusion->encode_jpeg(frame, jpeg, quality);
}

bool IrStreamPipeline::upload_frame() {
    if (jpeg.empty()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(uplink_mu_);
        uplink_jpeg_ = jpeg;
        uplink_has_job_ = true;
    }
    uplink_cv_.notify_one();
    return true;
}

void IrStreamPipeline::start_worker() {
    uplink_stop_.store(false);
    uplink_worker_ = std::thread([this]() {
        while (true) {
            std::vector<std::uint8_t> job;
            {
                std::unique_lock<std::mutex> lock(uplink_mu_);
                uplink_cv_.wait(lock, [this]() {
                    return uplink_has_job_ || uplink_stop_.load();
                });
                if (uplink_stop_.load() && !uplink_has_job_) {
                    break;
                }
                job.swap(uplink_jpeg_);
                uplink_has_job_ = false;
            }

            if (uplink == nullptr || job.empty()) {
                continue;
            }

            if (uplink->post_ir_frame(cfg.stream_id, job)) {
                ++frames_posted;
                if (frames_posted == 1 || frames_posted % 30 == 0) {
                    std::cout
                        << "ir_stream posted #" << frames_posted
                        << " jpeg_bytes=" << job.size()
                        << " ir=" << (camera != nullptr && camera->demo_mode ? "demo" : "live")
                        << '\n';
                }
            } else {
                ++frames_failed;
                if (frames_failed <= 3 || frames_failed % 30 == 0) {
                    std::cout
                        << "ir_stream post failed: " << uplink->last_error()
                        << '\n';
                }
            }
        }
    });
}

void IrStreamPipeline::stop_worker() {
    uplink_stop_.store(true);
    uplink_cv_.notify_all();
    if (uplink_worker_.joinable()) {
        uplink_worker_.join();
    }
    uplink_stop_.store(false);
    uplink_has_job_ = false;
    uplink_jpeg_.clear();
}
