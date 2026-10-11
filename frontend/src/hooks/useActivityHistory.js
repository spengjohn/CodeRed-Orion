import { useEffect, useState } from 'react'
import { getHistory } from '../api/readings'
import { TIMELINE_ZOOMS } from '../utils/timeline'

// Readings for the night timeline at a zoom level (see TIMELINE_ZOOMS): the
// zoom's whole scroll-back range, bucketed for its scale. Refetches when the
// zoom changes; the previous zoom's data stays visible while the next loads.
export function useActivityHistory(zoom) {
  const [state, setState] = useState({ zoom: null, data: [], error: null })

  useEffect(() => {
    let cancelled = false
    const { loadMs, bucketS } = TIMELINE_ZOOMS[zoom]
    getHistory(loadMs / 1000, bucketS)
      .then((data) => !cancelled && setState({ zoom, data, error: null }))
      .catch((error) => !cancelled && setState({ zoom, data: [], error }))
    return () => {
      cancelled = true
    }
  }, [zoom])

  return { data: state.data, loading: state.zoom !== zoom, error: state.error }
}
