import socket
import threading
import json
import logging
import os
from app.realtime import socketio

logger = logging.getLogger(__name__)

# Global flag to control whether we broadcast frames to frontend
streaming_active = False

# 🔒 安全防護 3: Edge 通關密語 (由環境變數設定)
EDGE_SECRET = os.getenv("EDGE_SECRET", "hackathon-secret")

def _handle_edge_client(conn, addr):
    global streaming_active
    buffer = ""
    authenticated = False
    logger.info(f"[Video Relay] Edge connected from {addr}")
    
    while True:
        try:
            data = conn.recv(8192).decode('utf-8')
            if not data:
                break
                
            buffer += data
            
            # 🔒 安全防護 4: 避免記憶體塞爆 (OOM Crash)，限制 Buffer 最大 1MB
            if len(buffer) > 1024 * 1024:
                logger.warning(f"Buffer overflow from {addr}, dropping connection.")
                break

            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                
                # 🔒 安全防護 5: 要求 Edge 先傳送密碼敲門
                if not authenticated:
                    if line.strip() == f"AUTH {EDGE_SECRET}":
                        authenticated = True
                        conn.sendall(b"AUTH_OK\n")
                        logger.info(f"[Video Relay] Edge authenticated: {addr}")
                    else:
                        logger.warning(f"[Video Relay] Edge auth failed: {addr}")
                        conn.sendall(b"AUTH_FAILED\n")
                        return  # 密碼錯直接中斷連線
                    continue

                # 已驗證，正常廣播影片
                if streaming_active:
                    try:
                        frame_data = json.loads(line)
                        socketio.emit("video_frame", frame_data)
                    except json.JSONDecodeError:
                        pass
                        
        except Exception as e:
            logger.error(f"[Video Relay] Connection error: {e}")
            break
            
    conn.close()
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
