import { getHistory, getLatest, normalizeReading } from './readings'

const mockFetch = (body) => {
  globalThis.fetch = jest.fn().mockResolvedValue({ ok: true, status: 200, json: async () => body })
}

test('normalizeReading converts ts from seconds to milliseconds', () => {
  expect(normalizeReading({ ts: 1760112000, babyWeightKg: 4.2 })).toEqual({
    ts: 1760112000000,
    babyWeightKg: 4.2,
  })
})

test('getHistory requests range and bucket in seconds and returns the normalized series', async () => {
  mockFetch({ range: 3600, bucket: 10, series: [{ ts: 1760112000 }] })

  const result = await getHistory(3600, 10)

  expect(fetch).toHaveBeenCalledWith('/api/readings/history?range=3600&bucket=10', expect.any(Object))
  expect(result).toEqual([{ ts: 1760112000000 }])
})

test('getLatest returns one normalized reading', async () => {
  mockFetch({ ts: 1760112000, isMoving: false })

  expect(await getLatest()).toEqual({ ts: 1760112000000, isMoving: false })
  expect(fetch).toHaveBeenCalledWith('/api/readings/latest', expect.any(Object))
})
