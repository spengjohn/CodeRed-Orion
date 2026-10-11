import { useEffect, useState } from 'react'
import { connectFrames } from '../api/live'

const TICK_MS = 1000

// The newest camera frame from the crib, as an <img> src. The feed counts as
// live while frames keep arriving; staleness uses when each frame arrived,
// not its ts, so the edge device's clock doesn't matter.
// The backend sends frame as a ready data URL; bare base64 is accepted too.
export function useCribFrames({ staleAfterMs = 5000 } = {}) {
  const [frame, setFrame] = useState(null)
  const [now, setNow] = useState(() => Date.now())

  useEffect(() => {
    const close = connectFrames(({ frame: jpeg }) => {
      if (typeof jpeg === 'string' && jpeg !== '') {
        const src = jpeg.startsWith('data:') ? jpeg : `data:image/jpeg;base64,${jpeg}`
        setFrame({ src, receivedAt: Date.now() })
      }
    })
    const tick = setInterval(() => setNow(Date.now()), TICK_MS)
    return () => {
      close()
      clearInterval(tick)
    }
  }, [])

  const isLive = frame !== null && Math.max(now, frame.receivedAt) - frame.receivedAt < staleAfterMs
  return { src: frame?.src ?? null, isLive }
}
