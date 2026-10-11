"""
REST API endpoints for managing emergency alert recipients.

Provides POST/GET/DELETE on /api/alerts/recipients so judges and parents
can register their phone numbers for live demo alerts.
"""

import re
import logging
from flask import Blueprint, jsonify, request
from app.auth.auth import require_auth

from app.alerts.recipients import add_recipient, get_recipients, clear_recipients

logger = logging.getLogger(__name__)

bp = Blueprint("alert_routes", __name__, url_prefix="/api/alerts")

# Basic E.164 validation: + followed by 7-15 digits
_PHONE_RE = re.compile(r"^\+[1-9]\d{6,14}$")


@bp.route("/recipients", methods=["POST"])
@require_auth
def add_phone():
    """
    Register a phone number to receive emergency alerts.

    Request JSON:
        {
            "phone": "+14155552671",
            "name": "Judge Michael"  (optional)
        }

    Returns 201 with the created entry on success.
    """
    data = request.get_json(silent=True) or {}
    phone = (data.get("phone") or "").strip()
    name = (data.get("name") or "").strip()

    if not phone:
        return jsonify({"error": "'phone' is required"}), 400

    if not _PHONE_RE.match(phone):
        return jsonify({"error": "Invalid phone format. Use E.164: +1XXXXXXXXXX"}), 400

    entry = add_recipient(phone, name)
    return jsonify({"message": "Recipient registered", "recipient": entry}), 201


@bp.route("/recipients", methods=["GET"])
@require_auth
def list_phones():
    """
    List all active (non-expired) registered phone numbers.

    Returns 200 with the list of recipient entries.
    """
    recipients = get_recipients()
    return jsonify({"recipients": recipients, "count": len(recipients)}), 200


@bp.route("/recipients", methods=["DELETE"])
@require_auth
def clear_phones():
    """
    Clear all registered phone numbers.

    Used to reset for a fresh judging/demo session.
    Returns 200 with the number of entries removed.
    """
    removed = clear_recipients()
    return jsonify({"message": "All recipients cleared", "removed": removed}), 200
