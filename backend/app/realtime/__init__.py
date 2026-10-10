"""Live push: sends the latest reading to authenticated connected browsers once a second."""

import threading
from flask import request
from flask_socketio import SocketIO, emit
from app.data import get_latest
from app.auth.auth import validate_user_token

PUSH_INTERVAL_S = 1

socketio = SocketIO(async_mode="threading")

_push_started = False
_push_lock = threading.Lock()


def _push_vitals():
    """Background loop: broadcast the newest reading to all clients."""
    while True:
        socketio.emit("vitals", get_latest())
        socketio.sleep(PUSH_INTERVAL_S)


@socketio.on("connect")
def on_connect(auth):
    token = None
    if auth and isinstance(auth, dict) and "token" in auth:
        token = auth["token"]
    else:
        auth_header = request.headers.get("Authorization", "")
        if auth_header.startswith("Bearer "):
            token = auth_header.removeprefix("Bearer ").strip()

    if not token:
        return False

    try:
        validate_user_token(token)
    except Exception:
        return False

    global _push_started
    with _push_lock:
        if not _push_started:
            socketio.start_background_task(_push_vitals)
            _push_started = True
            return

    emit("vitals", get_latest())
