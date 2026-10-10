import socket
import threading
import json
import logging
import os
from app.realtime import socketio

logger = logging.getLogger(__name__)

# Global flag to control whether we broadcast frames to frontend
streaming_active = False

EDGE_SECRET = os.getenv("EDGE_SECRET", "hackathon-secret")

def _handle_edge_client(conn, addr):
    global streaming_active
    buffer = bytearray()
    authenticated = False
    logger.info(f"[Video Relay] Edge connected from {addr}")
    
    
    conn.settimeout(15.0)

    try:
        while True:
            try:
                
                chunk = conn.recv(16384)
                if not chunk:
                    break
                    
                buffer.extend(chunk)
                
                
                if len(buffer) > 2 * 1024 * 1024:
                    logger.warning(f"[Video Relay] Buffer overflow from {addr} (>2MB), dropping connection.")
                    break

                while b"\n" in buffer:
                    newline_idx = buffer.find(b"\n")
                    line_bytes = bytes(buffer[:newline_idx])
                    del buffer[:newline_idx + 1]
                    
                    if not line_bytes:
                        continue

                    try:
                        line = line_bytes.decode("utf-8").strip()
                    except UnicodeDecodeError as decode_err:
                        logger.warning(f"[Video Relay] Corrupted UTF-8 payload from {addr}: {decode_err}")
                        continue
                    
                    if not line:
                        continue

                    
                    if not authenticated:
                        if line == f"AUTH {EDGE_SECRET}":
                            authenticated = True
                            conn.sendall(b"AUTH_OK\n")
                            logger.info(f"[Video Relay] Edge authenticated: {addr}")
                        else:
                            logger.warning(f"[Video Relay] Edge auth failed: {addr}")
                            conn.sendall(b"AUTH_FAILED\n")
                            return  
                        continue

                    if streaming_active:
                        try:
                            frame_data = json.loads(line)
                            socketio.emit("video_frame", frame_data)
                        except json.JSONDecodeError:
                            logger.debug("[Video Relay] Incomplete or invalid JSON frame, discarded.")
                            pass

            except socket.timeout:
                logger.warning(f"[Video Relay] Connection timeout from {addr}")
                break
            except Exception as e:
                logger.error(f"[Video Relay] Connection error: {e}")
                break
    finally:
        try:
            conn.close()
        except Exception:
            pass
        logger.info(f"[Video Relay] Edge disconnected {addr}")

def _tcp_server_loop(port):
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", port))
    server.listen(5)
    logger.info(f"[Video Relay] TCP Server listening on port {port} for Edge camera...")
    
    while True:
        conn, addr = server.accept()
        threading.Thread(target=_handle_edge_client, args=(conn, addr), daemon=True).start()

def start_relay(port=8765):
    threading.Thread(target=_tcp_server_loop, args=(port,), daemon=True).start()
