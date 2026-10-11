import { useMemo } from 'react'
import { activitySeries, activityStats } from '../utils/series'
import { RANGES, useReadings } from './useReadings'

// Movement data for the activity page: chart points (% of readings moving
// per bucket) and summary stats for the chosen range. Each history point
// covers one backend bucket, so durations use the bucket length.
export function useActivity(range) {
  const { data, loading, error } = useReadings(range)
  const intervalMs = RANGES[range].bucketS * 1000
  const series = useMemo(() => activitySeries(data), [data])
  const stats = useMemo(() => activityStats(data, intervalMs), [data, intervalMs])
  return { series, stats, loading, error }
}
