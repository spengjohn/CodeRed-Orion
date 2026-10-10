from flask import Blueprint, jsonify
from app.auth.auth import require_auth
import app.realtime.video_relay as video_relay

bp = Blueprint("video", __name__, url_prefix="/api/video")

@bp.post("/start")
@require_auth
def start_stream():
    """
    Frontend hits this API when parent clicks "Turn on camera".
    """
    video_relay.streaming_active = True
    return jsonify({
        "status": "success",
        "message": "Video stream activated. Backend is now forwarding frames."
    })


@bp.post("/stop")
@require_auth
def stop_stream():
    """
    Frontend hits this API when parent closes the camera.
    """
    video_relay.streaming_active = False
    return jsonify({
        "status": "success",
        "message": "Video stream deactivated. Forwarding stopped."
    })

