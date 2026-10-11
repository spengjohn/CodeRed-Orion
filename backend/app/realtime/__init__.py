"""Live push to authenticated browsers: checks for a new reading every
POLL_INTERVAL_S seconds and pushes it only when it is new (its ts changed).

The board writes to Tiger Data about once a minute, so reading every 30 s means
each new reading is pushed once, at most 30 s after it was written.

Clients must send an Auth0 token when connecting, e.g.
    io(url, { auth: { token } }).on("vitals", reading => ...)
The payload is the same JSON as GET /api/readings/latest.
"""

import logging
import threading
from flask import request
from flask_socketio import SocketIO, emit
from app.data import get_latest
from app.auth.auth import validate_user_token

POLL_INTERVAL_S = 30

logger = logging.getLogger(__name__)
socketio = SocketIO(async_mode="threading")

_push_started = False
_push_lock = threading.Lock()
_last_pushed_ts = None  # ts of the last reading pushed; the only thing kept in memory


def _safe_latest():
    """Latest reading, or None if there is no data yet or the database errored.

    Never raises, so a dropped database connection can't kill the push loop.
    """
    try:
        return get_latest()
    except Exception as e:
        logger.warning("Could not read latest reading: %s", e)
        return None


def _push_vitals():
    """Background loop: read the latest reading and broadcast it if it is new."""
    global _last_pushed_ts
    while True:
        reading = _safe_latest()
        if reading is not None and reading["ts"] != _last_pushed_ts:
            socketio.emit("vitals", reading)
            _last_pushed_ts = reading["ts"]
        socketio.sleep(POLL_INTERVAL_S)


@socketio.on("connect")
def on_connect(auth):
    global _push_started

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

    # Start the loop on the first connection only, so there is exactly one.
    # Its first read is new by definition, so it reaches this client immediately.
    with _push_lock:
        if not _push_started:
            socketio.start_background_task(_push_vitals)
            _push_started = True
            return

    reading = _safe_latest()  # later clients get the current reading right away
    if reading is not None:
        emit("vitals", reading)
