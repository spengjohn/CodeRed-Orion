"""
REST API endpoints for emergency alerts.

Provides POST/GET/DELETE on /api/alerts/recipients so judges and parents
can register their phone numbers for live demo alerts, plus the alert log
(GET/DELETE /api/alerts) and each alert's snapshot image for the frontend.
"""

import re
import logging
from flask import Blueprint, Response, jsonify, request
from app.auth.auth import require_auth

from app.alerts.recipients import add_recipient, get_recipients, clear_recipients
from app.alerts.db import delete_alerts, get_alert_snapshot, list_alerts
from app.data.format import to_alert_record

logger = logging.getLogger(__name__)

bp = Blueprint("alert_routes", __name__, url_prefix="/api/alerts")

# Basic E.164 validation: + followed by 7-15 digits
_PHONE_RE = re.compile(r"^\+[1-9]\d{6,14}$")

MAX_ALERTS = 200  # cap per request for the alert log


@bp.route("", methods=["GET"])
@require_auth
def alert_log():
    """
    The alert log, newest first: GET /api/alerts?limit=50  (limit 1-200, default 50)

    Returns 200 {"alerts": [...], "count": n}. Images aren't included; each alert
    with an image has a snapshotUrl to fetch it from (login required).
    """
    try:
        limit = int(request.args.get("limit", 50))
    except ValueError:
        return jsonify({"error": "limit must be an integer"}), 400
    if not 1 <= limit <= MAX_ALERTS:
        return jsonify({"error": f"limit must be between 1 and {MAX_ALERTS}"}), 400

    rows = list_alerts(limit)
    if rows is None:
        return jsonify({"error": "alert log unavailable (database not reachable)"}), 503

    alerts = [
        to_alert_record(alert_id, code, ts, detail, has_snapshot)
        for alert_id, code, detail, ts, has_snapshot in rows  # column order from list_alerts()
    ]
    return jsonify({"alerts": alerts, "count": len(alerts)}), 200


@bp.route("", methods=["DELETE"])
@require_auth
def clear_alert_log():
    """
    Delete alerts from the log: DELETE /api/alerts?before=<unix seconds>

    Only alerts at or before `before` are removed, so ones that arrived after the
    user loaded the page are kept. Without it, every alert is removed.
    Returns 200 {"message": ..., "removed": n}.
    """
    before = request.args.get("before")
    if before is not None:
        try:
            before = int(before)
        except ValueError:
            return jsonify({"error": "before must be an integer (unix seconds)"}), 400

    removed = delete_alerts(before)
    if removed is None:
        return jsonify({"error": "alert log unavailable (database not reachable)"}), 503
    return jsonify({"message": "Alerts cleared", "removed": removed}), 200


@bp.route("/<int:alert_id>/snapshot", methods=["GET"])
@require_auth
def alert_snapshot(alert_id: int):
    """One alert's camera snapshot as a JPEG: GET /api/alerts/<id>/snapshot (404 if none)."""
    image = get_alert_snapshot(alert_id)
    if image is None:
        return jsonify({"error": "no snapshot for this alert"}), 404
    return Response(image, mimetype="image/jpeg")


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
