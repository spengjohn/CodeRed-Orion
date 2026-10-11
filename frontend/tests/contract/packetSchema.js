// The reading shape the backend sends (backend/app/data/format.py to_reading),
// shared by the contract tests. If the backend changes it, update this file
// and the frontend together.

export const READING_FIELDS = {
  ts: 'number', // unix seconds, integer, UTC
  babyWeightKg: 'number',
  ambientTempC: 'number',
  babyTempC: 'number',
  breathsPerMin: 'number',
  isMoving: 'boolean',
}

// Physically plausible ranges; anything outside suggests a unit or sensor bug.
const RANGES = {
  babyTempC: [30, 43],
  ambientTempC: [0, 45],
  babyWeightKg: [0.5, 30],
  breathsPerMin: [0, 120],
}

export function expectValidReading(reading) {
  expect(Object.keys(reading).sort()).toEqual(Object.keys(READING_FIELDS).sort())

  for (const [field, type] of Object.entries(READING_FIELDS)) {
    expect(typeof reading[field]).toBe(type)
  }

  // Seconds, not milliseconds: a ms value would be ~1000x larger.
  expect(Number.isInteger(reading.ts)).toBe(true)
  expect(reading.ts).toBeLessThan(1e11)

  for (const [field, [min, max]] of Object.entries(RANGES)) {
    expect(reading[field]).toBeGreaterThanOrEqual(min)
    expect(reading[field]).toBeLessThanOrEqual(max)
  }
}

// GET /api/readings/history response: { range, bucket, series: [reading] }.
export function expectValidHistory(body, { range, bucket }) {
  expect(body).toMatchObject({ range, bucket })
  expect(Array.isArray(body.series)).toBe(true)
  body.series.forEach(expectValidReading)
}
