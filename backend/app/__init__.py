import os
import logging
from flask import Flask, jsonify
from flask_cors import CORS

# Set up basic logging so auth module messages are visible
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

def create_app():
    app = Flask(__name__)
    
    frontend_url = os.getenv("FRONTEND_URL", "http://localhost:1234")
    CORS(app, origins=[frontend_url])

    from app.api.readings import bp as readings_bp
    app.register_blueprint(readings_bp)

    from app.api.auth_routes import bp as auth_bp
    app.register_blueprint(auth_bp)

    from app.api.video import bp as video_bp
    app.register_blueprint(video_bp)

    # Log Auth0 config at startup (no secrets printed).
    from app.auth.auth import auth0_config_summary
    try:
        logger.info("Auth0 config: %s", auth0_config_summary())
    except ValueError as e:
        logger.warning("Auth0 not configured (%s); protected routes will return 401", e)

    from app.realtime import socketio
    socketio.init_app(app, cors_allowed_origins=[frontend_url])

    # Start Edge TCP Relay Server for video frames
    from app.realtime.video_relay import start_relay
    
    if os.environ.get("WERKZEUG_RUN_MAIN") == "true" or not app.debug:
        edge_port = int(os.getenv("EDGE_TCP_PORT", 8765))
        start_relay(port=edge_port)

    @app.get("/api/health")
    def health():
        return jsonify({"status": "ok"})

    return app
