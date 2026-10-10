"""
Single place that defines the JSON shape the frontend receives.

Both the dummy source and the (future) Tiger Data source build readings with
to_reading(), so switching sources never changes what the frontend sees.

Units: ts = unix seconds, weight = kg, temperatures = Celsius.
is_moving is set by the board (1/0) and sent as true/false.

"""

def to_reading(unix_time, baby_weight, ambient_temp, baby_temp, breaths_per_minute, is_moving):
    """Turn one row (same column order as the Tiger Data table) into a JSON-ready dict."""
    return {
        "ts": int(unix_time),
        "babyWeightKg": round(baby_weight, 2),
        "ambientTempC": round(ambient_temp, 1),
        "babyTempC": round(baby_temp, 1),
        "breathsPerMin": round(breaths_per_minute),
        "isMoving": bool(is_moving),
    }
