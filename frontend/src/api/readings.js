import { get } from './client'

// The backend's reading shape is defined in backend/app/data/format.py:
// { ts, babyWeightKg, ambientTempC, babyTempC, breathsPerMin, isMoving }.
// The backend sends ts in unix seconds; the frontend keeps ts in milliseconds
// so values can go straight into Date and Intl. This is the only conversion.
export function normalizeReading(reading) {
  return { ...reading, ts: reading.ts * 1000 }
}

// The most recent reading.
export async function getLatest() {
  return normalizeReading(await get('/readings/latest'))
}

// Readings for the last rangeS seconds, averaged by the backend into
// bucketS-second buckets, oldest first. Each ts is the start of its bucket.
export async function getHistory(rangeS, bucketS) {
  const { series } = await get(`/readings/history?range=${rangeS}&bucket=${bucketS}`)
  return series.map(normalizeReading)
}
