import os
from app import create_app
from app.realtime import socketio

app = create_app()

if __name__ == "__main__":
    
    host = os.getenv("HOST")
    port = int(os.getenv("PORT"))
    debug = os.getenv("DEBUG", "False").lower() in ("true", "1", "t")

    socketio.run(
        app,
        host=host,
        port=port,
        debug=debug
    )   