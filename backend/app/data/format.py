"""
Single place that defines the JSON shape the frontend receives.

Both the dummy source and the (future) Tiger Data source build readings with
to_reading(), so switching sources never changes what the frontend sees.

Units: ts = unix seconds, weight = kg, temperatures = Celsius.
is_moving (the board's `moved` column) is sent as true/false.
All decimal values are rounded to 2 places here, and only here, for both the
dummy and the Tiger Data source (this also tidies history averages).
A missing or invalid value (None, NaN, infinity) is sent as null: NaN is not
valid JSON and breaks JSON.parse in the browser.

"""

import math


def _two_dp(value):
    """float() fixes the type (int, Decimal, ... -> float); then round to 2 decimals.

    Returns None (JSON null) for missing or invalid values.
    """
    if value is None:
        return None
    number = float(value)
    if math.isnan(number) or math.isinf(number):
        return None
    return round(number, 2)


def to_reading(unix_time, baby_weight, ambient_temp, baby_temp, breaths_per_minute, is_moving):
    """Turn one row (same column order as the Tiger Data table) into a JSON-ready dict."""
    return {
        "ts": int(unix_time),
        "babyWeightKg": _two_dp(baby_weight),
        "ambientTempC": _two_dp(ambient_temp),
        "babyTempC": _two_dp(baby_temp),
        "breathsPerMin": _two_dp(breaths_per_minute),
        "isMoving": bool(is_moving),
    }


def to_alert(emergency_type, unix_time, details, snapshot_bytes_or_str):
    """
    Format an emergency alert into a JSON-ready dict for React frontends.

    Image is provided as a standard Data URL (RFC 2397) so React components
    can render directly via `<img src={alert.image} />` without any decoding.
    """
    if isinstance(snapshot_bytes_or_str, (bytes, bytearray)):
        import base64
        b64 = base64.b64encode(snapshot_bytes_or_str).decode("ascii")
        img_url = f"data:image/jpeg;base64,{b64}" if b64 else ""
    elif isinstance(snapshot_bytes_or_str, str):
        if snapshot_bytes_or_str and not snapshot_bytes_or_str.startswith("data:"):
            img_url = f"data:image/jpeg;base64,{snapshot_bytes_or_str}"
        else:
            img_url = snapshot_bytes_or_str
    else:
        img_url = ""

    t_val = int(unix_time) if unix_time else 0
    e_type = str(emergency_type or "UNKNOWN")

    return {
        "id": f"alert-{t_val}",
        "ts": t_val,
        "type": e_type,
        "emergencyType": e_type,
        "image": img_url,
        "snapshot": img_url,
        "details": details if isinstance(details, dict) else {},
    }


def to_alert_record(alert_id, code, unix_time, detail, has_snapshot):
    """
    One saved alert (a row of the alerts table) for the frontend's alert log.

    The image isn't included; the frontend loads it from snapshotUrl (login required).
    `detail` is the JSON text stored by the backend; it's turned back into a dict.
    """
    import json
    from app.alerts.codes import alert_description

    try:
        details = json.loads(detail) if detail else {}
    except (TypeError, ValueError):
        details = {}
    if not isinstance(details, dict):
        details = {}

    return {
        "id": int(alert_id),
        "code": int(code),
        "description": alert_description(code),
        "ts": int(unix_time) if unix_time else 0,
        "details": details,
        "hasSnapshot": bool(has_snapshot),
        "snapshotUrl": f"/api/alerts/{int(alert_id)}/snapshot" if has_snapshot else None,
    }


def to_video_frame(unix_time, frame_str, frame_id=0):
    """
    Format a video livestream frame into a JSON-ready dict for React frontends.
    """
    if isinstance(frame_str, (bytes, bytearray)):
        import base64
        b64 = base64.b64encode(frame_str).decode("ascii")
        img_url = f"data:image/jpeg;base64,{b64}"
    elif isinstance(frame_str, str):
        img_url = frame_str if frame_str.startswith("data:") else f"data:image/jpeg;base64,{frame_str}"
    else:
        img_url = ""

    return {
        "ts": int(unix_time) if unix_time else 0,
        "frame": img_url,
        "frameId": int(frame_id or 0),
    }

