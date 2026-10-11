// Helpers for the dashboard's night timeline (components/NightTimeline.jsx).
// All times are in milliseconds.

const MINUTE = 60 * 1000
const HOUR = 60 * MINUTE
const DAY = 24 * HOUR

// Zoom levels. Each loads more history than it shows, so the timeline can be
// scrolled back: visibleMs fits the screen, loadMs is how far back it scrolls.
// bucketS keeps every load under the backend's 2000-point cap.
export const TIMELINE_ZOOMS = {
  '6h': { label: '6 h', visibleMs: 6 * HOUR, loadMs: DAY, bucketS: 60, tickMs: HOUR },
  '24h': { label: '24 h', visibleMs: DAY, loadMs: 7 * DAY, bucketS: 360, tickMs: 3 * HOUR },
  '7d': { label: '7 d', visibleMs: 7 * DAY, loadMs: 30 * DAY, bucketS: 1800, tickMs: DAY },
}

// How much of a reading's bucket the baby spent moving (0-1). The Tiger Data
// history sends movingShare; otherwise (dummy data, live readings) fall back to
// isMoving, which means "any movement in the bucket".
export function movingShare(reading) {
  if (typeof reading.movingShare === 'number') return Math.min(1, Math.max(0, reading.movingShare))
  return reading.isMoving ? 1 : 0
}

// Segments for the activity band. Each reading covers [ts, ts + bucketMs).
// Consecutive still buckets merge into one segment; each moving bucket is its
// own segment with its share, so the band can show how much movement there was.
// A gap longer than one bucket ends a segment, so missing data shows as empty
// track instead of being bridged. Returns [{ start, end, moving, share }], oldest first.
export function activitySegments(readings, bucketMs) {
  const segments = []
  for (const r of [...readings].sort((a, b) => a.ts - b.ts)) {
    const start = r.ts
    const end = r.ts + bucketMs
    const share = movingShare(r)
    const moving = share > 0
    const last = segments.at(-1)
    if (!moving && last && !last.moving && start <= last.end + 1) {
      last.end = Math.max(last.end, end)
    } else {
      segments.push({ start, end, moving, share })
    }
  }
  return segments
}

// Group items whose positions (px) are closer than minPx, so overlapping alert
// pins become one pin with a count. Returns [{ x, items }], left to right.
export function groupByDistance(items, positionOf, minPx) {
  const groups = []
  for (const item of [...items].sort((a, b) => positionOf(a) - positionOf(b))) {
    const x = positionOf(item)
    const last = groups.at(-1)
    if (last && x - last.x < minPx) last.items.push(item)
    else groups.push({ x, items: [item] })
  }
  return groups
}

// Tick times every stepMs inside [startMs, endMs], aligned to local clock time
// (whole minutes or hours, or local midnight for day steps).
export function axisTicks(startMs, endMs, stepMs) {
  const first = new Date(startMs)
  if (stepMs >= DAY) {
    first.setHours(0, 0, 0, 0)
  } else if (stepMs >= HOUR) {
    first.setMinutes(0, 0, 0)
    const stepHours = Math.round(stepMs / HOUR)
    first.setHours(Math.floor(first.getHours() / stepHours) * stepHours)
  } else {
    first.setSeconds(0, 0)
    const stepMinutes = Math.max(1, Math.round(stepMs / MINUTE))
    first.setMinutes(Math.floor(first.getMinutes() / stepMinutes) * stepMinutes)
  }
  const ticks = []
  for (let t = first.getTime(); t <= endMs; ) {
    if (t >= startMs) ticks.push(t)
    // Step in local time so day ticks stay on midnight across DST changes.
    const next = new Date(t)
    if (stepMs >= DAY) next.setDate(next.getDate() + Math.round(stepMs / DAY))
    else if (stepMs >= HOUR) next.setHours(next.getHours() + Math.round(stepMs / HOUR))
    else next.setMinutes(next.getMinutes() + Math.max(1, Math.round(stepMs / MINUTE)))
    t = next.getTime()
  }
  return ticks
}

// Tick spacing for the analytics charts, picked so a range gets ~5-8 labels.
export function chartTickStep(rangeMs) {
  if (rangeMs <= 2 * HOUR) return 15 * MINUTE
  if (rangeMs <= DAY) return 4 * HOUR
  if (rangeMs <= 7 * DAY) return DAY
  if (rangeMs <= 14 * DAY) return 2 * DAY
  return 5 * DAY
}

// Local midnights and noons strictly inside (startMs, endMs), where 12-hour
// time switches between AM and PM. Returns [{ ts, period }], oldest first.
export function meridiemBoundaries(startMs, endMs) {
  const boundaries = []
  const t = new Date(startMs)
  t.setHours(t.getHours() < 12 ? 12 : 24, 0, 0, 0)
  while (t.getTime() < endMs) {
    if (t.getTime() > startMs) boundaries.push({ ts: t.getTime(), period: t.getHours() === 12 ? 'PM' : 'AM' })
    t.setHours(t.getHours() === 12 ? 24 : 12)
  }
  return boundaries
}

// Night spans (8 PM to 7 AM local time) overlapping [startMs, endMs], clipped to it.
export function nightBands(startMs, endMs, { fromHour = 20, toHour = 7 } = {}) {
  const bands = []
  const day = new Date(startMs)
  day.setHours(0, 0, 0, 0)
  day.setDate(day.getDate() - 1) // a night that started the evening before
  while (day.getTime() <= endMs) {
    const start = new Date(day)
    start.setHours(fromHour, 0, 0, 0)
    const end = new Date(day)
    end.setDate(end.getDate() + 1)
    end.setHours(toHour, 0, 0, 0)
    if (end.getTime() > startMs && start.getTime() < endMs) {
      bands.push({ start: Math.max(start.getTime(), startMs), end: Math.min(end.getTime(), endMs) })
    }
    day.setDate(day.getDate() + 1)
  }
  return bands
}

// Totals for the summary line over [fromMs, toMs]: time spent moving (each
// bucket's overlap with the window × its moving share) and the number of alerts.
export function summarize(readings, alerts, fromMs, toMs, bucketMs) {
  let movingMs = 0
  for (const r of readings) {
    const share = movingShare(r)
    if (share === 0) continue
    const overlap = Math.min(r.ts + bucketMs, toMs) - Math.max(r.ts, fromMs)
    if (overlap > 0) movingMs += overlap * share
  }
  const alertCount = alerts.filter((a) => a.ts >= fromMs && a.ts <= toMs).length
  return { movingMs, alertCount }
}
