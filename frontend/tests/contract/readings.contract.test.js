import { api, authHeaders, requireBackend } from './backend.js'
import { expectValidHistory, expectValidReading } from './packetSchema.js'

beforeAll(requireBackend)

// Readings will need an Auth0 token (frontend README, Backend Request 3). Until
// the backend adds @require_auth, this fails on purpose to flag the request.
test('GET /api/readings/latest rejects requests without a token', async () => {
  const res = await api().get('/api/readings/latest')
  expect(res.status).toBe(401)
})

test('GET /api/readings/latest returns one valid reading', async () => {
  const res = await api().get('/api/readings/latest').set(authHeaders())

  expect(res.status).toBe(200)
  expect(res.headers['content-type']).toMatch(/json/)
  expectValidReading(res.body)
})

describe('GET /api/readings/history', () => {
  // The ranges and buckets the frontend requests (src/hooks/useReadings.js).
  test.each([
    [3600, 60],
    [86400, 300],
    [7 * 86400, 1800],
    [14 * 86400, 3600],
    [30 * 86400, 7200],
  ])('range=%i bucket=%i returns a valid series', async (range, bucket) => {
    const res = await api().get('/api/readings/history').set(authHeaders()).query({ range, bucket })

    expect(res.status).toBe(200)
    expectValidHistory(res.body, { range, bucket })
    expect(res.body.series.length).toBeGreaterThan(0)
    expect(res.body.series.length).toBeLessThanOrEqual(2000)
  })

  test('series is oldest first, aligned to bucket starts, and within the range', async () => {
    const range = 3600
    const bucket = 60
    const res = await api().get('/api/readings/history').set(authHeaders()).query({ range, bucket })
    const ts = res.body.series.map((r) => r.ts)
    const now = Math.floor(Date.now() / 1000)

    expect(ts).toEqual([...ts].sort((a, b) => a - b))
    for (const t of ts) {
      expect(t % bucket).toBe(0)
      expect(t).toBeGreaterThanOrEqual(now - range - bucket)
      expect(t).toBeLessThanOrEqual(now + 5)
    }
  })

  test.each([
    ['non-integer values', { range: 'hour', bucket: 60 }],
    ['bucket larger than range', { range: 60, bucket: 3600 }],
    ['range over the maximum', { range: 31 * 86400, bucket: 3600 }],
    ['too many points', { range: 86400, bucket: 1 }],
  ])('rejects %s with 400', async (_, query) => {
    const res = await api().get('/api/readings/history').set(authHeaders()).query(query)
    expect(res.status).toBe(400)
    expect(res.body).toHaveProperty('error')
  })
})
