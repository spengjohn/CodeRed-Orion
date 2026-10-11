import { useFormatters } from '../hooks/useFormatters'
import { useCribFrames } from '../hooks/useCribFrames'

// The dashboard's camera tile. Frames arrive up to ~12 times a second, so this
// lives in its own component to keep those updates from re-rendering the page.
function CribVideo({ now }) {
  const fmt = useFormatters()
  const { src, isLive } = useCribFrames()

  return (
    <div className="video">
      {src && isLive ? <img className="feed" src={src} alt="Nursery camera" /> : 'Camera offline'}
      <span className={isLive ? 'live on' : 'live'}>{isLive ? 'Live' : 'Offline'}</span>
      <span className="cam">Nursery cam · {fmt.time(now)}</span>
    </div>
  )
}

export default CribVideo
