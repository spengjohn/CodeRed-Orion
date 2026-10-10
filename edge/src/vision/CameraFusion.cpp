#include "CameraFusion.hpp"

bool CameraFusion::init() {
    return true;
}

void CameraFusion::shutdown() {}

bool CameraFusion::render_ir(const OV5647& camera, RgbFrame& out) {
    (void)camera;
    out.width = out_width;
    out.height = out_height;
    out.rgb.assign(static_cast<std::size_t>(out_width * out_height * 3), 0);
    return true;
}

bool CameraFusion::render_fusion(
    const OV5647& camera,
    const MLX90640Driver& thermal,
    RgbFrame& out
) {
    (void)camera;
    (void)thermal;
    out.width = out_width;
    out.height = out_height;
    out.rgb.assign(static_cast<std::size_t>(out_width * out_height * 3), 0);
    return true;
}

bool CameraFusion::encode_jpeg(
    const RgbFrame& frame,
    std::vector<std::uint8_t>& jpeg_out,
    int quality
) {
    (void)frame;
    (void)quality;
    jpeg_out.clear();
    return true;
}
