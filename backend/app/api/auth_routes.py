"""Auth0 routes (moved from backend/app.py): one public, one protected."""

import os
import json
from datetime import datetime
from flask import Blueprint, g, jsonify, request

from app.auth.auth import require_auth

bp = Blueprint("auth_routes", __name__, url_prefix="/api")

# Minimal database path (stored in backend/users.json)
USERS_DB_FILE = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))), "users.json")


@bp.get("/public")
def public():
    """
    Public route — no Auth0 verification required.
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
    """
    return jsonify({
        "status": "success",
        "message": "Authorized! Welcome to private crib data 🛏️",
        "user_sub": g.user_sub,
    })


@bp.post("/auth/login")
def login():
    """
    Hackathon-specific Login API (Unauthenticated):
    Frontend passes user info in JSON body directly.
    """
    data = request.json or {}
    user_id = data.get("user_id", "guest_" + str(int(datetime.now().timestamp())))
    email = data.get("email", "no_email_provided")
    nickname = data.get("nickname", "User")
    
    users_db = {}
    if os.path.exists(USERS_DB_FILE):
        try:
            with open(USERS_DB_FILE, "r", encoding="utf-8") as f:
                users_db = json.load(f)
        except Exception:
            pass

    users_db[user_id] = {
        "user_id": user_id,
        "email": email,
        "nickname": nickname,
        "last_login": datetime.now().isoformat()
    }

    with open(USERS_DB_FILE, "w", encoding="utf-8") as f:
        json.dump(users_db, f, indent=4, ensure_ascii=False)

    return jsonify({
        "status": "success",
        "message": "Login successful, user recorded in DB.",
        "user": users_db[user_id]
    }), 200
