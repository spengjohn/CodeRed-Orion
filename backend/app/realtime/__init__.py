"""Live push: sends the latest reading to every connected browser once a second.

Frontend: io("http://localhost:5000").on("vitals", reading => ...)
The payload is the same JSON as GET /api/readings/latest.
"""

import threading

from flask_socketio import SocketIO, emit

from app.data import get_latest

PUSH_INTERVAL_S = 1

socketio = SocketIO(cors_allowed_origins="*", async_mode="threading")

_push_started = False
_push_lock = threading.Lock()


def _push_vitals():
    """Background loop: broadcast the newest reading to all clients."""
    while True:
        socketio.emit("vitals", get_latest())
        socketio.sleep(PUSH_INTERVAL_S)


@socketio.on("connect")
def on_connect():
    global _push_started

    # Start the loop on the first connection only, so there is exactly one.
    # Its first broadcast reaches this client immediately.
    with _push_lock:
        if not _push_started:
            socketio.start_background_task(_push_vitals)
            _push_started = True
            return

    emit("vitals", get_latest())  # later clients get a reading right away too
