#include "IrStreamPipeline.hpp"

bool IrStreamPipeline::init(
    const Config& config,
    OV5647& cam,
    CameraFusion& fus,
    UplinkClient& client
) {
    cfg = config;
    camera = &cam;
    fusion = &fus;
    uplink = &client;
    frames_posted = 0;
    frames_failed = 0;
    ready = camera != nullptr && fusion != nullptr && uplink != nullptr;
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
    ready = false;
    camera = nullptr;
    fusion = nullptr;
    uplink = nullptr;
    jpeg.clear();
}

bool IrStreamPipeline::render_frame() {
    return true;
}

bool IrStreamPipeline::encode_frame() {
    return true;
}

bool IrStreamPipeline::upload_frame() {
    return true;
}
