// Helpers for working with arrays of readings (ts in ms, field names as the
// backend sends them; see src/api/readings.js).

export const METRIC_KEYS = ['babyTempC', 'ambientTempC', 'babyWeightKg', 'breathsPerMin']

// One snapshot per minute (matches SNAPSHOT_INTERVAL_MS in hooks/useReadings).
const DEFAULT_INTERVAL_MS = 60 * 1000

const mean = (values) => values.reduce((sum, v) => sum + v, 0) / values.length

// Reduce a long series to at most maxPoints by averaging fixed-size buckets,
// so charts stay fast. A bucket counts as moving if any reading in it was
// moving. The backend already buckets history, so this rarely kicks in.
export function downsample(readings, maxPoints = 500) {
  if (readings.length <= maxPoints) return readings
  const size = Math.ceil(readings.length / maxPoints)
  const result = []
  for (let i = 0; i < readings.length; i += size) {
    const bucket = readings.slice(i, i + size)
    const point = {
      ts: bucket[0].ts,
      isMoving: bucket.some((r) => r.isMoving),
    }
    for (const key of METRIC_KEYS) {
      const values = bucket.map((r) => r[key]).filter((v) => typeof v === 'number')
      point[key] = values.length ? mean(values) : null
    }
    result.push(point)
  }
  return result
}

// Latest, min, max, and average of one metric. Pass excludeMoving to ignore
// readings taken while the child was moving (weight is unreliable then). If
// every reading is moving, which happens on long ranges because the backend
// marks a whole bucket moving when any movement occurred, all readings are used.
export function summarize(readings, key, { excludeMoving = false } = {}) {
  const still = excludeMoving ? readings.filter((r) => !r.isMoving) : readings
  const values = (still.length ? still : readings)
    .map((r) => r[key])
    .filter((v) => typeof v === 'number')
  if (!values.length) return { latest: null, min: null, max: null, avg: null }
  return {
    latest: values.at(-1),
    min: Math.min(...values),
    max: Math.max(...values),
    avg: mean(values),
  }
}

// Change in one metric from the first to the last reading, e.g. for "−0.2°
// since 1 hr". excludeMoving works as in summarize(). Null without two values.
export function change(readings, key, { excludeMoving = false } = {}) {
  const still = excludeMoving ? readings.filter((r) => !r.isMoving) : readings
  const valid = (still.length ? still : readings).filter((r) => typeof r[key] === 'number')
  if (valid.length < 2) return null
  const first = valid[0]
  const last = valid.at(-1)
  return { delta: last[key] - first[key], fromTs: first.ts, toTs: last.ts }
}

// How long the child has been in their current state (moving or still), for
// "12 min since moving". atLeast is true when the state covers every reading,
// so the real duration may be longer than the data shows.
export function currentActivity(readings, intervalMs = DEFAULT_INTERVAL_MS) {
  const latest = readings.at(-1)
  if (!latest) return null
  const last = movingRanges(readings, intervalMs).at(-1)
  const start = latest.isMoving ? last.start : (last?.end ?? readings[0].ts)
  return {
    isMoving: latest.isMoving,
    ms: latest.ts + intervalMs - start,
    atLeast: start === readings[0].ts,
  }
}

// Percentage of readings taken while moving, per chart bucket (0-100), for the
// activity chart. Each backend bucket is already 0 or 100 (moving if any
// minute in it had movement), so this is only finer when it groups several.
export function activitySeries(readings, maxPoints = 500) {
  const size = Math.max(1, Math.ceil(readings.length / maxPoints))
  const result = []
  for (let i = 0; i < readings.length; i += size) {
    const bucket = readings.slice(i, i + size)
    const moving = bucket.filter((r) => r.isMoving).length
    result.push({ ts: bucket[0].ts, movingPct: (moving / bucket.length) * 100 })
  }
  return result
}

// Movement summary for a range: number of episodes, total and longest
// episode length (ms), and the share of readings taken while moving.
// intervalMs is how long each reading covers (see movingRanges).
export function activityStats(readings, intervalMs = DEFAULT_INTERVAL_MS) {
  const durations = movingRanges(readings, intervalMs).map((r) => r.end - r.start)
  const movingCount = readings.filter((r) => r.isMoving).length
  return {
    episodes: durations.length,
    totalMs: durations.reduce((sum, d) => sum + d, 0),
    longestMs: durations.length ? Math.max(...durations) : 0,
    percent: readings.length ? (movingCount / readings.length) * 100 : null,
  }
}

// Time spans where the child was moving, for shading charts and durations.
// Each reading covers intervalMs starting at its ts (one minute for raw
// snapshots, the bucket length for history), so a span ends at the first
// still reading after it, or one interval after its last moving reading.
export function movingRanges(readings, intervalMs = DEFAULT_INTERVAL_MS) {
  const ranges = []
  let start = null
  readings.forEach((r, i) => {
    if (r.isMoving && start === null) start = r.ts
    if (!r.isMoving && start !== null) {
      ranges.push({ start, end: r.ts })
      start = null
    }
    if (start !== null && i === readings.length - 1) {
      ranges.push({ start, end: r.ts + intervalMs })
    }
  })
  return ranges
}
