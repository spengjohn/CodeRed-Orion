"""Dummy data source: fake but realistic vitals until Tiger Data is live.

Every value is a pure function of the unix second it belongs to:
    value(t) = baseline + slow sine wave + small noise seeded by t
So the same second always gives the same reading. That keeps "latest" and
"history" consistent with each other and stops charts from changing on refresh,
without storing anything.
"""

import math
import random
import time

from .format import to_reading

# Each bucket is averaged from at most this many samples, so long ranges stay fast.
MAX_SAMPLES_PER_BUCKET = 10


def _wave(t, period_s):
    return math.sin(2 * math.pi * t / period_s)


def _vitals(t):
    """Raw (unrounded) vitals for second t, in table column order."""
    rng = random.Random(t)  # same t -> same noise
    baby_weight = 4.20 + rng.uniform(-0.01, 0.01)                              # kg, nearly flat
    ambient_temp = 22.5 + 0.8 * _wave(t, 7200) + rng.uniform(-0.1, 0.1)        # C, slow room drift (2 h)
    baby_temp = 36.8 + 0.15 * _wave(t, 1800) + rng.uniform(-0.05, 0.05)        # C, small drift (30 min)
    breaths_per_minute = 40 + 4 * _wave(t, 300) + rng.uniform(-2, 2)           # newborn resting ~36-44 (5 min)
    return baby_weight, ambient_temp, baby_temp, breaths_per_minute


def _is_moving(t):
    """Baby wriggles for 10 s every 4 minutes."""
    return t % 240 < 10


def get_latest():
    """Most recent reading. Same shape as one row of get_history()."""
    now = int(time.time())
    return to_reading(now, *_vitals(now), _is_moving(now))


def get_history(range_s=3600, bucket_s=60):
    """Readings for the last range_s seconds, averaged into bucket_s-second buckets.

    Mirrors what the Tiger Data query will do with time_bucket() + avg().
    isMoving is true if the baby moved at any point in the bucket (like bool_or()).
    Each item's ts is the start of its bucket, oldest first.
    """
    now = int(time.time())
    start = now - range_s
    first_bucket = start - start % bucket_s  # align buckets to clean boundaries
    step = max(1, bucket_s // MAX_SAMPLES_PER_BUCKET)

    series = []
    for bucket_start in range(first_bucket, now + 1, bucket_s):
        bucket_end = min(bucket_start + bucket_s, now + 1)
        samples = [_vitals(t) for t in range(bucket_start, bucket_end, step)]
        averages = [sum(column) / len(samples) for column in zip(*samples)]
        moved = any(_is_moving(t) for t in range(bucket_start, bucket_end))  # check every second so short moves aren't missed
        series.append(to_reading(bucket_start, *averages, moved))
    return series
