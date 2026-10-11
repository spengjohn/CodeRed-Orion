"""
Emergency Alert Dispatcher.

Coordinates three actions when an emergency is triggered:
1. Persist the record to the Tiger Data database, then
2. push the alert + snapshot to the frontend via Socket.IO (with the saved row's id,
   so it matches the alert log from GET /api/alerts).
3. In parallel: voice calls (and SMS if enabled) to all registered numbers via Twilio.

Both run in background threads so the TCP relay is never blocked.
"""

import base64
import logging
import threading
from typing import Optional

from app.realtime import socketio
from app.data.format import to_alert
from app.alerts.codes import alert_description
from app.alerts.db import save_emergency_alert
from app.alerts.recipients import get_recipients
from app.alerts.twilio_client import dispatch_alerts

logger = logging.getLogger(__name__)

# Cache latest alert in memory so newly connected React clients receive current state immediately
_latest_alert = None
_latest_alert_lock = threading.Lock()


def get_latest_alert() -> Optional[dict]:
    """Return the most recent active emergency alert, or None."""
    with _latest_alert_lock:
        return _latest_alert


def _emit_socketio(
    emergency_type: str,
    timestamp: int,
    details: dict,
    snapshot_bytes: bytes,
    db_id: Optional[int] = None,
) -> None:
    """Push the emergency alert to all connected frontend clients."""
    global _latest_alert
    try:
        # Build pure, standard JSON payload matching React interface expectations
        alert_payload = to_alert(
            emergency_type=emergency_type,
            unix_time=timestamp,
            details=details,
            snapshot_bytes_or_str=snapshot_bytes,
        )
        # Extra fields for the alert log page: readable text, and the saved row's id
        # (same as "id" in GET /api/alerts, so the page can tell it already has this alert).
        try:
            code = int(emergency_type)
        except (ValueError, TypeError):
            code = None
        alert_payload.update({
            "code": code,
            "description": alert_description(emergency_type),
            "dbId": db_id,
            "snapshotUrl": f"/api/alerts/{db_id}/snapshot" if db_id is not None and snapshot_bytes else None,
        })

        with _latest_alert_lock:
            _latest_alert = alert_payload

        socketio.emit("emergency_alert", alert_payload)
        logger.info("[Dispatcher] Socket.IO emergency_alert emitted: type=%s dbId=%s", emergency_type, db_id)
    except Exception as exc:
        logger.error("[Dispatcher] Socket.IO emit failed: %s", exc)


def _save_then_emit(emergency_type: str, timestamp: int, details: dict, snapshot_bytes: bytes) -> None:
    """Save the alert to the database (best-effort), then push it with the saved id."""
    db_id = None
    try:
        db_id = save_emergency_alert(emergency_type, details, snapshot_bytes, timestamp)
    except Exception as exc:
        logger.error("[Dispatcher] DB save failed: %s", exc)
    _emit_socketio(emergency_type, timestamp, details, snapshot_bytes, db_id)


def _send_twilio(emergency_type: str, timestamp: int, details: dict) -> None:
    """Send voice calls and SMS to all registered recipients."""
    try:
        recipients = get_recipients()
        phones = [r["phone"] for r in recipients if r.get("phone")]
        dispatch_alerts(phones, emergency_type, timestamp, details)
    except Exception as exc:
        logger.error("[Dispatcher] Twilio dispatch failed: %s", exc)


def handle_emergency(
    emergency_type: str,
    timestamp: int,
    details: dict,
    snapshot_bytes: bytes,
) -> None:
    """
    Main entry point: dispatch an emergency alert through all channels.

    This function is designed to be called from the TCP relay handler.
    It fires all three dispatch actions in background threads so the
    caller (TCP read loop) is never blocked.

    Parameters
    ----------
    emergency_type : str
        Alert type string (e.g. 'BREATHING_PAUSE').
    timestamp : int
        Unix timestamp from the edge device.
    details : dict
        Free-form details dict from the edge payload.
    snapshot_bytes : bytes
        Raw binary camera/thermal snapshot.
    """
    logger.info(
        "[Dispatcher] 🚨 EMERGENCY RECEIVED: type=%s ts=%d snapshot=%d bytes",
        emergency_type,
        timestamp,
        len(snapshot_bytes),
    )

    # Save then push (one thread), and Twilio calls in parallel, non-blocking
    threading.Thread(
        target=_save_then_emit,
        args=(emergency_type, timestamp, details, snapshot_bytes),
        daemon=True,
    ).start()

    threading.Thread(
        target=_send_twilio,
        args=(emergency_type, timestamp, details),
        daemon=True,
    ).start()
