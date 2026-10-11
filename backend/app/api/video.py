from flask import Blueprint, jsonify
from app.auth.auth import require_auth
import app.realtime.video_relay as video_relay

bp = Blueprint("video", __name__, url_prefix="/api/video")

@bp.get("/status")
def stream_status():
    """
    Check current video relay status and configured FPS.
    """
    return jsonify({
        "status": "success",
        "streaming_active": video_relay.streaming_active,
        "target_fps": video_relay.TARGET_FPS,
        "message": "Live video relay is continuously active by default."
    })


@bp.post("/start")
@require_auth
def start_stream():
    """
    Optional: Frontend can still call this if needed, but streaming is now active by default.
    """
    video_relay.streaming_active = True
    return jsonify({
        "status": "success",
        "message": "Video stream is active."
    })


@bp.post("/stop")
@require_auth
def stop_stream():
    """
    Frontend hits this API if parent explicitly wants to pause server-side forwarding.
    """
    video_relay.streaming_active = False
    return jsonify({
        "status": "success",
        "message": "Video stream deactivated. Forwarding stopped."
    })

