import { useEffect } from 'react'
import { useFormatters } from '../hooks/useFormatters'
import { useCribFrames } from '../hooks/useCribFrames'

// The dashboard's camera tile. Frames arrive up to ~12 times a second, so this
// lives in its own component to keep those updates from re-rendering the page.
// onOfflineChange(isOffline) is called only when the camera goes offline or
// comes back.
function CribVideo({ now, onOfflineChange }) {
  const fmt = useFormatters()
  const { src, isLive, isOffline } = useCribFrames()

  useEffect(() => onOfflineChange?.(isOffline), [isOffline, onOfflineChange])

  return (
    <div className="video">
      {src && isLive ? <img className="feed" src={src} alt="Nursery camera" /> : 'Camera offline'}
      <span className={isLive ? 'live on' : 'live'}>{isLive ? 'Live' : 'Offline'}</span>
      <span className="cam">{fmt.time(now)}</span>
    </div>
  )
}

export default CribVideo
