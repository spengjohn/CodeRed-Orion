"""Auth0 routes (moved from backend/app.py): one public, one protected."""

from flask import Blueprint, g, jsonify

from app.auth.auth import require_auth

bp = Blueprint("auth_routes", __name__, url_prefix="/api")


@bp.get("/public")
def public():
    """
    Public route — no Auth0 verification required.
    Useful for health checks or frontend connectivity tests.
    """
    return jsonify({
        "status": "success",
        "message": "Welcome to CodeRED Orion API 🚀 (public endpoint)",
    })


@bp.get("/private-crib-data")
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
