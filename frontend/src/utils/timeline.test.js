import { activitySegments, axisTicks, groupByDistance, movingShare, nightBands, summarize, TIMELINE_ZOOMS } from './timeline'

const MIN = 60 * 1000
const HOUR = 60 * MIN
const reading = (ts, isMoving) => ({ ts, isMoving })

test('every zoom stays under the backend cap of 2000 points', () => {
  for (const { loadMs, bucketS, visibleMs } of Object.values(TIMELINE_ZOOMS)) {
    expect(loadMs / 1000 / bucketS).toBeLessThanOrEqual(2000)
    expect(loadMs).toBeGreaterThan(visibleMs) // there is always history to scroll back to
  }
})

test('merges consecutive buckets into moving / still segments', () => {
  const readings = [reading(0, false), reading(MIN, false), reading(2 * MIN, true), reading(3 * MIN, true), reading(4 * MIN, false)]
  expect(activitySegments(readings, MIN)).toEqual([
    { start: 0, end: 2 * MIN, moving: false, share: 0 },
    { start: 2 * MIN, end: 3 * MIN, moving: true, share: 1 },
    { start: 3 * MIN, end: 4 * MIN, moving: true, share: 1 },
    { start: 4 * MIN, end: 5 * MIN, moving: false, share: 0 },
  ])
})

test('a gap in the data splits a segment instead of bridging it', () => {
  const segments = activitySegments([reading(0, false), reading(10 * MIN, false)], MIN)
  expect(segments).toEqual([
    { start: 0, end: MIN, moving: false, share: 0 },
    { start: 10 * MIN, end: 11 * MIN, moving: false, share: 0 },
  ])
})

test('handles unsorted input and an empty list', () => {
  expect(activitySegments([], MIN)).toEqual([])
  expect(activitySegments([reading(MIN, false), reading(0, false)], MIN)).toEqual([{ start: 0, end: 2 * MIN, moving: false, share: 0 }])
})

test('axis ticks land on whole local hours and local midnight', () => {
  const start = new Date(2026, 9, 10, 13, 25).getTime()
  const hourly = axisTicks(start, start + 6 * HOUR, 3 * HOUR)
  expect(hourly.map((t) => new Date(t).getHours())).toEqual([15, 18])
  expect(hourly.every((t) => new Date(t).getMinutes() === 0)).toBe(true)

  const daily = axisTicks(start, start + 3 * 24 * HOUR, 24 * HOUR)
  expect(daily).toHaveLength(3)
  expect(daily.every((t) => new Date(t).getHours() === 0)).toBe(true)
})

test('night bands run 8 PM to 7 AM and are clipped to the window', () => {
  const start = new Date(2026, 9, 10, 18, 0).getTime() // 6 PM
  const end = new Date(2026, 9, 11, 9, 0).getTime() // 9 AM next day
  expect(nightBands(start, end)).toEqual([
    { start: new Date(2026, 9, 10, 20, 0).getTime(), end: new Date(2026, 9, 11, 7, 0).getTime() },
  ])
  const midNight = new Date(2026, 9, 11, 2, 0).getTime()
  expect(nightBands(midNight, midNight + HOUR)).toEqual([{ start: midNight, end: midNight + HOUR }])
})

test('summarize counts moving time and alerts inside the window only', () => {
  const readings = [reading(0, true), reading(MIN, true), reading(2 * MIN, false), reading(10 * MIN, true)]
  const alerts = [{ ts: 30 * 1000 }, { ts: 5 * MIN }, { ts: 20 * MIN }]
  expect(summarize(readings, alerts, 30 * 1000, 6 * MIN, MIN)).toEqual({ movingMs: 90 * 1000, alertCount: 2 })
})

test('uses movingShare when the history has it, isMoving otherwise', () => {
  expect(movingShare({ isMoving: true, movingShare: 0.25 })).toBe(0.25)
  expect(movingShare({ isMoving: true })).toBe(1)
  expect(movingShare({ isMoving: false })).toBe(0)
  const segs = activitySegments([{ ts: 0, isMoving: true, movingShare: 0.2 }], 30 * MIN)
  expect(segs).toEqual([{ start: 0, end: 30 * MIN, moving: true, share: 0.2 }])
  // 30 min bucket, 20% moving -> 6 min of movement
  expect(summarize([{ ts: 0, isMoving: true, movingShare: 0.2 }], [], 0, 30 * MIN, 30 * MIN).movingMs).toBe(6 * MIN)
})

test('groups alert pins that would overlap', () => {
  const alerts = [{ id: 1, x: 100 }, { id: 2, x: 110 }, { id: 3, x: 200 }, { id: 4, x: 50 }]
  const groups = groupByDistance(alerts, (a) => a.x, 28)
  expect(groups.map((g) => g.items.map((a) => a.id))).toEqual([[4], [1, 2], [3]])
  expect(groups.map((g) => g.x)).toEqual([50, 100, 200])
})
