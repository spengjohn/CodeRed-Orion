"""
Single place that defines the JSON shape the frontend receives.

Both the dummy source and the (future) Tiger Data source build readings with
to_reading(), so switching sources never changes what the frontend sees.

Units: ts = unix seconds, weight = kg, temperatures = Celsius.
is_moving (the board's `moved` column) is sent as true/false.
All decimal values are rounded to 2 places here, and only here, for both the
dummy and the Tiger Data source (this also tidies history averages).

"""

def _two_dp(value):
    """float() fixes the type (int, Decimal, ... -> float); then round to 2 decimals."""
    return round(float(value), 2)


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
