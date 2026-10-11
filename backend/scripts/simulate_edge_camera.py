"""Simulate Edge camera TCP streaming to backend (dev & demo tool).

Usage:
    python backend/scripts/simulate_edge_camera.py [host] [port] [secret]

Example:
    python backend/scripts/simulate_edge_camera.py localhost 8765 hackathon-secret
"""

import sys
import time
import socket
import json
import base64

# Simple 1x1 minimal valid JPEG header/data for fallback if PIL is not installed
TINY_JPEG_B64 = (
    "/9j/4AAQSkZJRgABAQEASABIAAD/2wBDAP////////////////////////////////////////////////"
    "//////////////////////////////////////wgALCAABAAEBAREA/8QAFBABAAAAAAAAAAAAAAAAAAAA"
    "AP/aAAgBAQABPxA="
)

def create_synthetic_frame_b64(frame_count):
    """
    Attempts to generate a real JPEG with time and counter using PIL.
    If PIL is not installed, falls back to a minimal valid base64 JPEG.
    """
    try:
        from PIL import Image, ImageDraw
        import io
        
        # 320x240 RGB canvas (dark thermal-like palette)
        img = Image.new("RGB", (320, 240), color=(20, 25, 45))
        draw = ImageDraw.Draw(img)
        
        # Draw a simulated baby silhouette / thermal blob
        draw.ellipse([110, 80, 210, 160], fill=(180, 70, 80))
        draw.text((10, 10), f"Simulated Edge Camera", fill=(255, 255, 255))
        draw.text((10, 30), f"Time: {time.strftime('%H:%M:%S')}", fill=(200, 200, 200))
        draw.text((10, 50), f"Frame: #{frame_count}", fill=(100, 255, 100))
        
        buf = io.BytesIO()
        img.save(buf, format="JPEG", quality=70)
        return base64.b64encode(buf.getvalue()).decode("ascii")
    except ImportError:
        return TINY_JPEG_B64

def main():
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8765
    secret = sys.argv[3] if len(sys.argv) > 3 else "hackathon-secret"

    print(f"[*] Connecting to Video Relay TCP at {host}:{port}...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((host, port))
    except Exception as e:
        print(f"[!] Failed to connect: {e}")
        return

    # Handshake
    print(f"[*] Sending AUTH handshake...")
    s.sendall(f"AUTH {secret}\n".encode("utf-8"))
    resp = s.recv(1024).decode("utf-8").strip()
    if resp != "AUTH_OK":
        print(f"[!] Authentication failed from server: {resp}")
        s.close()
        return

    print(f"[+] Authenticated successfully! Streaming frames at ~12 FPS (Ctrl+C to stop)...")
    frame_count = 0
    try:
        while True:
            frame_count += 1
            frame_b64 = create_synthetic_frame_b64(frame_count)
            payload = {
                "ts": int(time.time()),
                "frame": frame_b64,
                "frame_id": frame_count
            }
            # Newline delimited JSON
            data = (json.dumps(payload) + "\n").encode("utf-8")
            s.sendall(data)
            
            if frame_count % 30 == 0:
                print(f"[+] Sent frame #{frame_count} ({len(data)} bytes)")
                
            time.sleep(1.0 / 12)  # ~12 FPS
    except KeyboardInterrupt:
        print("\n[*] Stopped by user.")
    finally:
        s.close()
        print("[*] Connection closed.")

if __name__ == "__main__":
    main()

