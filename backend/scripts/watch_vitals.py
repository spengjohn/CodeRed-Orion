"""Print live vitals pushed by the backend (dev tool).

One-time install (not in requirements.txt):
    pip install "python-socketio[client]"
Run (backend must be running):
    python scripts/watch_vitals.py [url]
"""

import sys

import socketio

url = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:5000"
sio = socketio.Client()


@sio.on("vitals")
def on_vitals(reading):
    print(reading)


sio.connect(url)
print(f"Connected to {url}, Ctrl+C to stop")
sio.wait()
