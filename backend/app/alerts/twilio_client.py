"""
Twilio multi-recipient dispatcher for emergency alerts.

Sends voice calls and SMS messages to all registered phone numbers.
Includes a full mock mode: if Twilio credentials are missing or set to
'mock', all calls and messages are logged to the terminal instead of
hitting the Twilio API — perfect for development and demo rehearsal.
"""

import logging
import os
from pathlib import Path
from typing import List, Tuple
from dotenv import load_dotenv

# Explicitly load backend/.env
load_dotenv(Path(__file__).resolve().parent.parent.parent / ".env")

logger = logging.getLogger(__name__)


def _get_credentials() -> Tuple[str, str, str]:
    """Retrieve Twilio credentials dynamically from environment."""
    return (
        os.getenv("TWILIO_ACCOUNT_SID", "").strip(),
        os.getenv("TWILIO_AUTH_TOKEN", "").strip(),
        os.getenv("TWILIO_PHONE_NUMBER", "").strip(),
    )


def _is_mock_mode() -> bool:
    """Return True if Twilio should run in mock/log-only mode."""
    sid, token, from_phone = _get_credentials()
    if not sid or not token or not from_phone:
        return True
    if sid.lower() == "mock" or token.lower() == "mock":
        return True
    return False


def _get_client():
    """Lazily create and return a Twilio REST client."""
    from twilio.rest import Client
    sid, token, _ = _get_credentials()
    return Client(sid, token)


def _build_twiml(emergency_type: str, timestamp: int) -> str:
    """
    Build TwiML for a voice call announcing the emergency.

    Uses <Say> with a clear, urgent cadence appropriate for an infant
    monitoring alert.
    """
    message = (
        f"CodeRed Alert. An emergency of type {emergency_type} "
        f"has been detected by the infant monitoring system. "
        f"Please check on the baby immediately. "
        f"Repeating. {emergency_type} alert detected."
    )
    return (
        f'<Response>'
        f'<Say voice="alice" language="en-US" loop="2">{message}</Say>'
        f'</Response>'
    )


def _build_sms_body(emergency_type: str, timestamp: int, details: dict) -> str:
    """Build the SMS body for an emergency alert."""
    body = (
        f"\U0001F6A8 CODERED EMERGENCY ALERT \U0001F6A8\n\n"
        f"Type: {emergency_type}\n"
        f"Timestamp: {timestamp}\n"
    )
    if details:
        for key, val in details.items():
            body += f"{key}: {val}\n"
    body += "\nPlease check on the baby immediately."
    return body


def dispatch_alerts(
    phones: List[str],
    emergency_type: str,
    timestamp: int,
    details: dict,
) -> dict:
    """
    Send voice calls and SMS to every phone number in the list.

    Parameters
    ----------
    phones : list[str]
        E.164 formatted phone numbers.
    emergency_type : str
        Alert type string (e.g. 'BREATHING_PAUSE').
    timestamp : int
        Unix timestamp of the event.
    details : dict
        Extra detail fields from the edge device.

    Returns
    -------
    dict
        Summary with keys 'calls', 'sms', 'errors' counting outcomes.
    """
    results = {"calls": 0, "sms": 0, "errors": 0}

    if not phones:
        logger.info("[Twilio] No recipients registered — skipping dispatch.")
        return results

    mock = _is_mock_mode()
    if mock:
        logger.info("[Twilio] Running in MOCK mode (credentials not configured).")

    twiml = _build_twiml(emergency_type, timestamp)
    sms_body = _build_sms_body(emergency_type, timestamp, details)

    client = None if mock else _get_client()
    _, _, from_phone = _get_credentials()

    for phone in phones:
        # --- Voice Call ---
        try:
            if mock:
                logger.info(
                    "[Twilio][MOCK] VOICE CALL -> %s | TwiML: %s",
                    phone,
                    twiml[:120],
                )
            else:
                client.calls.create(
                    to=phone,
                    from_=from_phone,
                    twiml=twiml,
                )
                logger.info("[Twilio] Voice call placed to %s", phone)
            results["calls"] += 1
        except Exception as exc:
            logger.error("[Twilio] Voice call to %s failed: %s", phone, exc)
            results["errors"] += 1

        # --- SMS ---
        try:
            if mock:
                logger.info(
                    "[Twilio][MOCK] SMS -> %s | Body: %s",
                    phone,
                    sms_body[:120],
                )
            else:
                client.messages.create(
                    to=phone,
                    from_=from_phone,
                    body=sms_body,
                )
                logger.info("[Twilio] SMS sent to %s", phone)
            results["sms"] += 1
        except Exception as exc:
            logger.error("[Twilio] SMS to %s failed: %s", phone, exc)
            results["errors"] += 1

    logger.info(
        "[Twilio] Dispatch complete: %d calls, %d SMS, %d errors",
        results["calls"],
        results["sms"],
        results["errors"],
    )
    return results
