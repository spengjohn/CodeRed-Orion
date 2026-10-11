"""Print live vitals pushed by the backend (dev tool).

The live push requires an Auth0 access token. Put it in backend/.env:
    AUTH0_TEST_TOKEN=<access token>
(the backend also needs AUTH0_DOMAIN and AUTH0_AUDIENCE there to check it).

One-time install (not in requirements.txt):
    pip install "python-socketio[client]"
Run (backend must be running):
    python scripts/watch_vitals.py [url]
"""

import os
import sys
import time
from pathlib import Path

import socketio
from dotenv import load_dotenv

load_dotenv(Path(__file__).resolve().parent.parent / ".env")  # backend/.env

url = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:5000"
token = os.getenv("AUTH0_TEST_TOKEN", "").strip()
if not token:
    sys.exit("AUTH0_TEST_TOKEN is not set; add an Auth0 access token to backend/.env")

sio = socketio.Client()


@sio.on("vitals")
def on_vitals(reading):
    print(reading)


try:
    # wait_timeout: the first connection can take a few seconds while the backend
    # downloads Auth0's public keys to check the token (the default is only 1 s).
    sio.connect(url, auth={"token": token}, wait_timeout=10)
except socketio.exceptions.ConnectionError as e:
    sys.exit(f"Could not connect to {url}: {e}\n"
             "If the server is running, the token was rejected: check it hasn't expired and that "
             "AUTH0_DOMAIN / AUTH0_AUDIENCE in backend/.env match it.")

print(f"Connected to {url}, Ctrl+C to stop")
try:
    while True:
        time.sleep(1)  # short sleeps let Ctrl+C through on Windows
except KeyboardInterrupt:
    print("Stopping...")
finally:
    sio.disconnect()
