"""
Edge Camera Video Relay Service.

Listens on a dedicated TCP port for incoming camera/thermal frames from the
Edge device, authenticates the client, enforces rate limiting (10-12 FPS),
and broadcasts frames to connected frontend clients via Socket.IO.
"""

import base64
import binascii
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


def _alert_code(parsed: dict) -> str:
    """
    The alert's numeric code as a string ("4"), from whichever field the board sent:
    "emergency_code": 4, "emergency_type": 4, or "emergency_type": "CODE_4".
    Anything else is passed through unchanged (the alerts table then skips it).
    """
    for value in (parsed.get("emergency_code"), parsed.get("emergency_type")):
        if value is None:
            continue
        text = str(value).strip()
        if text.upper().startswith("CODE_"):
            text = text[5:]
        if text.lstrip("-").isdigit():
            return str(int(text))
    return str(parsed.get("emergency_type") or "UNKNOWN")


def _decode_snapshot(value) -> bytes:
    """A base64 snapshot string from the JSON (optionally a data: URL) as raw bytes; b"" if missing or invalid."""
    if not isinstance(value, str) or not value:
        return b""
    if value.startswith("data:"):
        value = value.split(",", 1)[-1]
    try:
        return base64.b64decode(value, validate=True)
    except (binascii.Error, ValueError):
        logger.warning("[Video Relay] Emergency snapshot is not valid base64 — ignored.")
        return b""


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

                    # Parse JSON payload
                    try:
                        parsed = json.loads(line)
                    except json.JSONDecodeError:
                        logger.debug("[Video Relay] Non-JSON payload discarded from %s", addr)
                        continue

                    if not isinstance(parsed, dict):
                        logger.debug("[Video Relay] Non-dict payload ignored.")
                        continue

                    # ── Emergency Packet Handler (Multiplexed on video TCP connection) ──
                    if parsed.get("event") == "emergency":
                        try:
                            snapshot_len = max(0, int(parsed.get("snapshot_len", 0)))
                        except (ValueError, TypeError):
                            snapshot_len = 0

                        if snapshot_len > MAX_BUFFER_BYTES:
                            logger.warning(
                                "[Video Relay] Snapshot length %d exceeds max buffer. Skipping snapshot.",
                                snapshot_len,
                            )
                            snapshot_len = 0

                        # Read exactly snapshot_len binary bytes from buffer / socket
                        while len(buffer) < snapshot_len:
                            more = conn.recv(16384)
                            if not more:
                                break
                            buffer.extend(more)

                        snapshot_bytes = bytes(buffer[:snapshot_len])
                        del buffer[:snapshot_len]

                        # Safe parsing of timestamp & details
                        ts_raw = parsed.get("timestamp")
                        try:
                            timestamp = int(float(ts_raw)) if ts_raw is not None else int(time.time())
                        except (ValueError, TypeError):
                            timestamp = int(time.time())

                        details = parsed.get("details")
                        if not isinstance(details, dict):
                            detail_str = parsed.get("detail") or parsed.get("details")
                            details = {"message": str(detail_str)} if detail_str else {}

                        emergency_code = parsed.get("emergency_code")
                        emergency_type = _alert_code(parsed)

                        # Support snapshot as raw trailing bytes or base64 string inside JSON;
                        # either way it's passed on as raw JPEG bytes.
                        snapshot_payload = snapshot_bytes or _decode_snapshot(parsed.get("snapshot"))

                        logger.info(
                            "[Video Relay] 🚨 Emergency received on video stream: type=%s (code=%s)",
                            emergency_type,
                            emergency_code,
                        )

                        from app.alerts.dispatcher import handle_emergency
                        handle_emergency(
                            emergency_type=emergency_type,
                            timestamp=timestamp,
                            details=details,
                            snapshot_bytes=snapshot_payload,
                        )
                        # Immediately continue so next video frames stream uninterrupted
                        continue

                    # ── Normal Video Frame (continuous streaming, rate-limited to ~10-12 FPS) ──
                    if streaming_active:
                        now = time.time()
                        if now - last_emit_time >= MIN_FRAME_INTERVAL:
                            last_emit_time = now
                            from app.data.format import to_video_frame
                            frame_payload = to_video_frame(
                                unix_time=parsed.get("ts", int(now)),
                                frame_str=parsed.get("frame", ""),
                                frame_id=parsed.get("frame_id") or parsed.get("frameId", 0),
                            )
                            socketio.emit("video_frame", frame_payload)

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
