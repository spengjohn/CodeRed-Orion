import { useEffect, useState } from 'react'
import { getHistory } from '../api/readings'

const HOUR_S = 60 * 60
const DAY_S = 24 * HOUR_S

// The crib board stores one snapshot per minute, so buckets below a minute
// would come back sparse and a moving row stands for up to a minute of movement.
export const SNAPSHOT_INTERVAL_MS = 60 * 1000

// Range options for the analytics pages. bucketS is the backend's averaging
// bucket: never below one snapshot, ~60-360 chart points per range.
// 30d is the backend's maximum range (MAX_RANGE_S).
export const RANGES = {
  '1h': { ms: HOUR_S * 1000, bucketS: 60 },
  '24h': { ms: DAY_S * 1000, bucketS: 300 },
  '7d': { ms: 7 * DAY_S * 1000, bucketS: 1800 },
  '14d': { ms: 14 * DAY_S * 1000, bucketS: 3600 },
  '30d': { ms: 30 * DAY_S * 1000, bucketS: 7200 },
}

// History for the analytics pages: readings from now back by `range`.
// While a new range loads, the previous range's data stays visible.
export function useReadings(range) {
  const [state, setState] = useState({ range: null, data: [], error: null })

  useEffect(() => {
    let cancelled = false
    const { ms, bucketS } = RANGES[range]
    getHistory(ms / 1000, bucketS)
      .then((data) => !cancelled && setState({ range, data, error: null }))
      .catch((error) => !cancelled && setState({ range, data: [], error }))
    return () => {
      cancelled = true
    }
  }, [range])

  return { data: state.data, loading: state.range !== range, error: state.error }
}
