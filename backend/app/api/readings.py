 
"""Readings endpoints: JSON for the frontend dashboard."""

import math

from flask import Blueprint, jsonify, request

from app.data import get_history, get_latest

bp = Blueprint("readings", __name__, url_prefix="/api/readings")

MAX_RANGE_S = 7 * 24 * 3600  # 7 days
MAX_POINTS = 2000            # cap chart points per response


@bp.get("/latest")
def latest():
    """GET /api/readings/latest -> one reading."""
    return jsonify(get_latest())


@bp.get("/history")
def history():
    """GET /api/readings/history?range=3600&bucket=60 (both in seconds)."""
    try:
        range_s = int(request.args.get("range", 3600))
        bucket_s = int(request.args.get("bucket", 60))
    except ValueError:
        return jsonify({"error": "range and bucket must be integers (seconds)"}), 400

    if not (1 <= bucket_s <= range_s <= MAX_RANGE_S):
        return jsonify({"error": f"need 1 <= bucket <= range <= {MAX_RANGE_S}"}), 400
    if range_s // bucket_s > MAX_POINTS:
        min_bucket = math.ceil(range_s / MAX_POINTS)
        return jsonify({"error": f"too many points; use a bucket of at least {min_bucket} s"}), 400

    return jsonify({
        "range": range_s,
        "bucket": bucket_s,
        "series": get_history(range_s, bucket_s),
    })
