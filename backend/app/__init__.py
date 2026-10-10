"""
Flask application factory. Run from backend/: flask --app app run --debug

Architecture:
  Nginx on the same EC2 handles all HTTPS traffic for whisker.health:
    - /api/*   → proxy_pass to this Flask app (port 5000)
    - /*       → static frontend files (/var/www/whisker-frontend)
  In production frontend and backend share the same origin; CORS is only
  needed for local development (Vite dev server on localhost:5173).
"""

import logging

from flask import Flask, jsonify
from flask_cors import CORS

# Set up basic logging so auth module messages are visible
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def create_app():
    app = Flask(__name__)
    CORS(app)

    from app.api.readings import bp as readings_bp
    app.register_blueprint(readings_bp)

    from app.api.auth_routes import bp as auth_bp
    app.register_blueprint(auth_bp)

    # Log Auth0 config at startup (no secrets printed). Without a .env the app
    # still starts: protected routes just return 401 until Auth0 is configured.
    from app.auth.auth import auth0_config_summary
    try:
        logger.info("Auth0 config: %s", auth0_config_summary())
    except ValueError as e:
        logger.warning("Auth0 not configured (%s); protected routes will return 401", e)

    from app.realtime import socketio
    socketio.init_app(app)

    @app.get("/api/health")
    def health():
        return jsonify({"status": "ok"})

    return app
