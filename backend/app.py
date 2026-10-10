"""
app.py — Flask application entry point

Architecture:
  Nginx on the same EC2 handles all HTTPS traffic for whisker.health:
    - /api/*   → proxy_pass to this Flask app (port 5000)
    - /*       → static frontend files (/var/www/whisker-frontend)
  Because frontend and backend share the same origin, CORS is not needed.
"""

import logging

from flask import Flask, g, jsonify

from app.auth.auth import auth0_config_summary, require_auth

# ─────────────────────────────────────────────
# Flask Initialization
# ─────────────────────────────────────────────
app = Flask(__name__)

# Set up basic logging so auth module messages are visible
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Log Auth0 config at startup (no secrets printed)
logger.info("Auth0 config: %s", auth0_config_summary())


# ─────────────────────────────────────────────
# Route Definitions
# ─────────────────────────────────────────────

@app.route("/api/public")
def public():
    """
    Public route — no Auth0 verification required.
    Useful for health checks or frontend connectivity tests.
    """
    return jsonify({
        "status": "success",
        "message": "Welcome to CodeRED Orion API 🚀 (public endpoint)",
    })


@app.route("/api/private-crib-data")
@require_auth
def private_crib_data():
    """
    Protected route — requires a valid Auth0 Bearer token.

    After @require_auth succeeds, decoded JWT claims are available on:
      - g.user_claims  (full claim dict)
      - g.user_sub     (Auth0 subject / user id)
    """
    return jsonify({
        "status": "success",
        "message": "Authorized! Welcome to private crib data 🛏️",
        "user_sub": g.user_sub,
    })


# ─────────────────────────────────────────────
# Application Runner
# ─────────────────────────────────────────────
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
