"""
Edge Camera Video Relay Service.

Listens on a dedicated TCP port for incoming camera/thermal frames from the
Edge device, authenticates the client, enforces rate limiting (10-12 FPS),
and broadcasts frames to connected frontend clients via Socket.IO.
"""

import hmac
import json
import logging
import os
import socket
import threading
import time
from typing import Tuple

from app.realtime import socketio

logger = logging.getLogger(__name__)

# Default streaming state: active immediately for hackathon / live demo simplicity.
streaming_active: bool = True

# Target frame rate cap (~10-12 FPS) to conserve network bandwidth and prevent client lag.
TARGET_FPS: int = 12
MIN_FRAME_INTERVAL: float = 1.0 / TARGET_FPS  # ~0.0833s

# Max buffer size before dropping connection to prevent Out-Of-Memory (OOM) crashes (2MB).
MAX_BUFFER_BYTES: int = 2 * 1024 * 1024

# Socket timeout in seconds to detect dropped or dead connections.
SOCKET_TIMEOUT_SECONDS: float = 15.0

_relay_started: bool = False
_relay_lock = threading.Lock()


def get_edge_secret() -> str:
    """Retrieve the Edge authentication secret token from environment variables."""
    return os.getenv("EDGE_SECRET", "hackathon-secret")


def _handle_edge_client(conn: socket.socket, addr: Tuple[str, int]) -> None:
    """
    Handle an incoming Edge camera TCP connection.

    Workflow:
    1. Authenticates client via 'AUTH <secret>' handshake.
    2. Buffers incoming binary stream safely and splits on newline delimiters.
    3. Throttles emissions to TARGET_FPS.
    4. Emits verified frames to Socket.IO clients under 'video_frame' event.
    """
    global streaming_active
    buffer = bytearray()
    authenticated = False
    last_emit_time = 0.0
    edge_secret = get_edge_secret()

    logger.info("[Video Relay] Edge connected from %s:%s", addr[0], addr[1])
    conn.settimeout(SOCKET_TIMEOUT_SECONDS)

    try:
        while True:
            try:
                # Read binary chunks in 16KB blocks
                chunk = conn.recv(16384)
                if not chunk:
                    break

                buffer.extend(chunk)

                # Security Guard: Prevent OOM by enforcing a maximum buffer threshold
                if len(buffer) > MAX_BUFFER_BYTES:
                    logger.warning(
                        "[Video Relay] Buffer overflow from %s (>%d bytes). Dropping connection.",
                        addr,
                        MAX_BUFFER_BYTES,
                    )
                    break

                # Process all complete lines delimited by newline
                while b"\n" in buffer:
                    newline_idx = buffer.find(b"\n")
                    line_bytes = bytes(buffer[:newline_idx])
                    del buffer[:newline_idx + 1]

                    if not line_bytes:
                        continue

                    # Safely decode UTF-8 payload without terminating connection on corruption
                    try:
                        line = line_bytes.decode("utf-8").strip()
                    except UnicodeDecodeError as decode_err:
                        logger.warning(
                            "[Video Relay] Corrupted UTF-8 payload from %s: %s", addr, decode_err
                        )
                        continue

                    if not line:
                        continue

                    # Security Guard: Require initial AUTH handshake
                    if not authenticated:
                        expected_auth = f"AUTH {edge_secret}"
                        if hmac.compare_digest(line, expected_auth):
                            authenticated = True
                            conn.sendall(b"AUTH_OK\n")
                            logger.info("[Video Relay] Edge successfully authenticated: %s", addr)
                        else:
                            logger.warning("[Video Relay] Edge authentication failed: %s", addr)
                            conn.sendall(b"AUTH_FAILED\n")
                            return
                        continue

                    # Forward video frames to frontend clients (rate-limited to ~10-12 FPS)
                    if streaming_active:
                        now = time.time()
                        if now - last_emit_time >= MIN_FRAME_INTERVAL:
                            last_emit_time = now
                            try:
                                frame_data = json.loads(line)
                                if isinstance(frame_data, dict):
                                    socketio.emit("video_frame", frame_data)
                                else:
                                    logger.debug("[Video Relay] Non-dictionary frame payload ignored.")
                            except json.JSONDecodeError:
                                logger.debug("[Video Relay] Incomplete or invalid JSON frame discarded.")

            except socket.timeout:
                logger.warning("[Video Relay] Connection timed out for %s", addr)
                break
            except Exception as exc:
                logger.error("[Video Relay] Client connection error from %s: %s", addr, exc)
                break

    finally:
        try:
            conn.close()
        except Exception:
            pass
        logger.info("[Video Relay] Edge disconnected from %s", addr)


def _tcp_server_loop(port: int) -> None:
    """Run the main TCP server listener loop."""
    try:
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("0.0.0.0", port))
        server.listen(5)
        logger.info("[Video Relay] TCP Server listening on port %d for Edge camera...", port)
    except OSError as err:
        logger.error("[Video Relay] Failed to bind TCP server on port %d: %s", port, err)
        return

    while True:
        try:
            conn, addr = server.accept()
            threading.Thread(
                target=_handle_edge_client, args=(conn, addr), daemon=True
            ).start()
        except Exception as exc:
            logger.error("[Video Relay] Server accept error: %s", exc)


def start_relay(port: int = 8765) -> None:
    """Start the TCP relay server in a background daemon thread (idempotent)."""
    global _relay_started
    with _relay_lock:
        if _relay_started:
            logger.debug("[Video Relay] TCP relay server is already running.")
            return
        _relay_started = True
        threading.Thread(target=_tcp_server_loop, args=(port,), daemon=True).start()
