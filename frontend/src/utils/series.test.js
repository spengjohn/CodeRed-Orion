import {
  activitySeries,
  activityStats,
  change,
  currentActivity,
  downsample,
  movingRanges,
  summarize,
} from './series'

const reading = (ts, overrides = {}) => ({
  ts,
  babyTempC: 36.8,
  ambientTempC: 21.5,
  babyWeightKg: 4.2,
  breathsPerMin: 38,
  isMoving: false,
  ...overrides,
})

describe('downsample', () => {
  test('returns short series unchanged', () => {
    const readings = [reading(0), reading(1)]
    expect(downsample(readings, 10)).toBe(readings)
  })

  test('caps the point count and averages each bucket', () => {
    const readings = Array.from({ length: 1000 }, (_, i) =>
      reading(i, { breathsPerMin: i, isMoving: i === 5 }),
    )
    const result = downsample(readings, 100)
    expect(result).toHaveLength(100)
    // First bucket is readings 0-9.
    expect(result[0]).toMatchObject({ ts: 0, breathsPerMin: 4.5, isMoving: true })
    expect(result[1].isMoving).toBe(false)
  })
})

describe('summarize', () => {
  const readings = [
    reading(0, { babyWeightKg: 4.0 }),
    reading(1, { babyWeightKg: 5.0, isMoving: true }),
    reading(2, { babyWeightKg: 4.2 }),
  ]

  test('returns latest, min, max, and average', () => {
    const stats = summarize(readings, 'babyWeightKg')
    expect(stats).toMatchObject({ latest: 4.2, min: 4.0, max: 5.0 })
    expect(stats.avg).toBeCloseTo(4.4)
  })

  test('can ignore readings taken while moving', () => {
    expect(summarize(readings, 'babyWeightKg', { excludeMoving: true })).toMatchObject({ max: 4.2 })
  })

  test('uses all readings when every reading was taken while moving', () => {
    const allMoving = readings.map((r) => ({ ...r, isMoving: true }))
    expect(summarize(allMoving, 'babyWeightKg', { excludeMoving: true })).toMatchObject({
      min: 4.0,
      max: 5.0,
    })
  })

  test('returns nulls for an empty series', () => {
    expect(summarize([], 'babyWeightKg')).toEqual({ latest: null, min: null, max: null, avg: null })
  })
})

const flagged = (flags) => flags.map((isMoving, i) => reading(i, { isMoving }))

describe('movingRanges', () => {
  test('merges consecutive moving readings; a trailing span lasts one interval', () => {
    // Readings at t=0..4, each covering 1 unit of time.
    expect(movingRanges(flagged([false, true, true, false, true]), 1)).toEqual([
      { start: 1, end: 3 },
      { start: 4, end: 5 },
    ])
  })

  test('a single moving minute snapshot spans one minute by default', () => {
    const MIN = 60 * 1000
    const snapshots = [false, true, false].map((isMoving, i) => reading(i * MIN, { isMoving }))
    expect(movingRanges(snapshots)).toEqual([{ start: MIN, end: 2 * MIN }])
    expect(movingRanges([reading(0, { isMoving: true })])).toEqual([{ start: 0, end: MIN }])
  })
})

describe('activitySeries', () => {
  test('gives 0 or 100 per reading when under maxPoints', () => {
    const result = activitySeries(flagged([false, true, false]))
    expect(result.map((p) => p.movingPct)).toEqual([0, 100, 0])
  })

  test('gives the share of moving readings per bucket', () => {
    // 100 readings into 10 buckets of 10; the first bucket has 3 moving.
    const readings = flagged(Array.from({ length: 100 }, (_, i) => i < 3))
    const result = activitySeries(readings, 10)
    expect(result).toHaveLength(10)
    expect(result[0]).toEqual({ ts: 0, movingPct: 30 })
    expect(result[1].movingPct).toBe(0)
  })
})

describe('activityStats', () => {
  test('counts episodes, durations, and share of time moving', () => {
    // Episodes: 1-3 (length 2) and 4-5 (length 1: series ends while moving,
    // so the last reading counts for one interval).
    expect(activityStats(flagged([false, true, true, false, true]), 1)).toEqual({
      episodes: 2,
      totalMs: 3,
      longestMs: 2,
      percent: 60,
    })
  })

  test('handles an empty series', () => {
    expect(activityStats([])).toEqual({ episodes: 0, totalMs: 0, longestMs: 0, percent: null })
  })
})

describe('change', () => {
  test('is the difference between the first and last values', () => {
    const readings = [reading(0, { babyTempC: 36.9 }), reading(1), reading(2, { babyTempC: 36.7 })]
    const result = change(readings, 'babyTempC')
    expect(result.delta).toBeCloseTo(-0.2)
    expect(result).toMatchObject({ fromTs: 0, toTs: 2 })
  })

  test('can skip moving readings, falling back to all if every one is moving', () => {
    const readings = [
      reading(0, { babyWeightKg: 9, isMoving: true }),
      reading(1, { babyWeightKg: 4.2 }),
      reading(2, { babyWeightKg: 4.3 }),
    ]
    expect(change(readings, 'babyWeightKg', { excludeMoving: true }).delta).toBeCloseTo(0.1)
    const allMoving = readings.map((r) => ({ ...r, isMoving: true }))
    expect(change(allMoving, 'babyWeightKg', { excludeMoving: true }).delta).toBeCloseTo(-4.7)
  })

  test('is null without two values', () => {
    expect(change([reading(0)], 'babyTempC')).toBeNull()
    expect(change([], 'babyTempC')).toBeNull()
  })
})

describe('currentActivity', () => {
  test('still: time since the last movement ended', () => {
    // Moving at t=1; still from t=2 through the latest reading at t=4 (+1 interval).
    expect(currentActivity(flagged([false, true, false, false, false]), 1)).toEqual({
      isMoving: false,
      ms: 3,
      atLeast: false,
    })
  })

  test('moving: length of the current episode', () => {
    expect(currentActivity(flagged([false, false, true, true]), 1)).toEqual({
      isMoving: true,
      ms: 2,
      atLeast: false,
    })
  })

  test('flags a state that covers all the data', () => {
    expect(currentActivity(flagged([false, false, false]), 1)).toEqual({
      isMoving: false,
      ms: 3,
      atLeast: true,
    })
    expect(currentActivity([], 1)).toBeNull()
  })
})
