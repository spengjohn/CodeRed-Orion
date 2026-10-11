import { useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react'
import { Link } from 'react-router'
import { alertKind, alertTitle } from '../api/alerts'
import { useActivityHistory } from '../hooks/useActivityHistory'
import { useAlerts } from '../hooks/useAlerts'
import { useAlertSnapshot } from '../hooks/useAlertSnapshot'
import { useFormatters } from '../hooks/useFormatters'
import { formatDuration } from '../utils/time'
import { activitySegments, axisTicks, groupByDistance, nightBands, summarize, TIMELINE_ZOOMS } from '../utils/timeline'
import AlertIcon from './AlertIcon'

const POPOVER_W = 280
const PIN_GAP_PX = 30 // pins closer than this merge into one with a count

function CloseButton({ onClose }) {
  return (
    <button type="button" className="tl-popover-close" aria-label="Close" onClick={onClose}>
      <svg viewBox="0 0 24 24" aria-hidden="true">
        <path d="M6 6l12 12M18 6 6 18" />
      </svg>
    </button>
  )
}

// Details for a pin: one alert in full (with its snapshot), or a list when
// several alerts were close together.
function AlertPopover({ alerts, left, onClose }) {
  const fmt = useFormatters()
  if (alerts.length > 1) {
    return (
      <div className="tl-popover" role="dialog" aria-label={`${alerts.length} alerts`} style={{ left, '--m': 'var(--alert)' }}>
        <CloseButton onClose={onClose} />
        <strong>{alerts.length} alerts</strong>
        <ul className="tl-popover-list">
          {alerts.map((a) => (
            <li key={a.id} style={{ '--m': `var(--${alertKind(a.code)})` }}>
              <span>{alertTitle(a.description)}</span>
              <span className="muted">{fmt.dateTime(a.ts)}</span>
            </li>
          ))}
        </ul>
        <Link to="/alerts">Open in alert log</Link>
      </div>
    )
  }
  return <SingleAlertPopover alert={alerts[0]} left={left} onClose={onClose} />
}

function SingleAlertPopover({ alert, left, onClose }) {
  const fmt = useFormatters()
  const { src, failed } = useAlertSnapshot(alert, true)
  return (
    <div
      className="tl-popover"
      role="dialog"
      aria-label={alertTitle(alert.description)}
      style={{ left, '--m': `var(--${alertKind(alert.code)})` }}
    >
      <CloseButton onClose={onClose} />
      <strong>{alertTitle(alert.description)}</strong>
      <span className="muted">{fmt.dateTime(alert.ts)}</span>
      {alert.details.message && <span>{alert.details.message}</span>}
      {alert.hasSnapshot &&
        (src ? (
          <img src={src} alt="Crib camera when the alert fired" />
        ) : failed ? (
          <span className="muted">Snapshot unavailable</span>
        ) : (
          <span className="skeleton tl-popover-img" aria-label="Loading snapshot" />
        ))}
      <Link to="/alerts">Open in alert log</Link>
    </div>
  )
}

// The dashboard's timeline: an activity band (moving vs. still) with alert pins
// on one time axis. Zoom sets how much fits on screen; drag or scroll to go back.
// liveReadings: the dashboard's live readings, so the right edge stays current.
// now: the dashboard's clock (ms).
function NightTimeline({ liveReadings = [], now }) {
  const fmt = useFormatters()
  const [zoom, setZoom] = useState('24h')
  const { visibleMs, loadMs, bucketS, tickMs } = TIMELINE_ZOOMS[zoom]
  const bucketMs = bucketS * 1000
  const history = useActivityHistory(zoom)
  const { alerts } = useAlerts({ limit: 200 })

  const scroller = useRef(null)
  const [width, setWidth] = useState(0)
  const [scrollLeft, setScrollLeft] = useState(0)
  const [selectedId, setSelectedId] = useState(null)
  const [mountedAt] = useState(() => Date.now())

  // Keep the track sized to the panel.
  useLayoutEffect(() => {
    const el = scroller.current
    const update = () => setWidth(el.clientWidth)
    update()
    const observer = new ResizeObserver(update)
    observer.observe(el)
    return () => observer.disconnect()
  }, [])

  const end = now
  const start = end - loadMs
  const pxPerMs = width / visibleMs
  const trackWidth = Math.round(loadMs * pxPerMs)
  const x = (t) => (t - start) * pxPerMs

  // Open at "now" (the right end) whenever the zoom or size changes.
  useLayoutEffect(() => {
    const el = scroller.current
    if (el) el.scrollLeft = el.scrollWidth
  }, [zoom, trackWidth])

  // History plus any live readings newer than it.
  const readings = useMemo(() => {
    const lastHistoric = history.data.at(-1)?.ts ?? -Infinity
    return [...history.data, ...liveReadings.filter((r) => r.ts > lastHistoric)]
  }, [history.data, liveReadings])

  const segments = useMemo(() => activitySegments(readings, bucketMs), [readings, bucketMs])
  const nights = useMemo(() => nightBands(start, end), [start, end])
  const ticks = useMemo(() => axisTicks(start, end, tickMs), [start, end, tickMs])
  const pins = alerts.filter((a) => a.ts >= start && a.ts <= end)
  const groups = groupByDistance(pins, (a) => x(a.ts), PIN_GAP_PX)

  // The window currently on screen, for the summary line.
  const viewFrom = start + scrollLeft / (pxPerMs || 1)
  const viewTo = Math.min(end, viewFrom + visibleMs)
  const atNow = end - viewTo < 60 * 1000
  const { movingMs, alertCount } = summarize(readings, alerts, viewFrom, viewTo, bucketMs)

  // Drag to pan (pins handle their own clicks).
  const drag = useRef(null)
  const onPointerDown = (e) => {
    if (e.button !== 0) return
    drag.current = { x: e.clientX, left: scroller.current.scrollLeft }
    e.currentTarget.setPointerCapture(e.pointerId)
  }
  const onPointerMove = (e) => {
    if (drag.current) scroller.current.scrollLeft = drag.current.left - (e.clientX - drag.current.x)
  }
  const endDrag = () => {
    drag.current = null
  }

  // Escape closes the alert details.
  useEffect(() => {
    if (!selectedId) return undefined
    const onKey = (e) => e.key === 'Escape' && setSelectedId(null)
    document.addEventListener('keydown', onKey)
    return () => document.removeEventListener('keydown', onKey)
  }, [selectedId])

  const selected = groups.find((g) => g.items[0].id === selectedId)
  const popoverLeft = selected
    ? Math.min(Math.max(selected.x - scrollLeft - POPOVER_W / 2, 0), Math.max(width - POPOVER_W, 0))
    : 0

  return (
    <section className="panel timeline">
      <div className="tl-head">
        <div>
          <h2>Timeline</h2>
          <span className="muted">
            {atNow ? `Last ${TIMELINE_ZOOMS[zoom].label}` : `${fmt.dateTime(viewFrom)} – ${fmt.dateTime(viewTo)}`}
            {' · '}moving {movingMs > 0 ? formatDuration(movingMs) : 'none'}
            {' · '}
            {alertCount === 1 ? '1 alert' : `${alertCount} alerts`}
          </span>
        </div>
        <div className="range-picker" role="group" aria-label="Timeline zoom">
          {Object.entries(TIMELINE_ZOOMS).map(([key, { label }]) => (
            <button
              key={key}
              type="button"
              aria-pressed={zoom === key}
              onClick={() => {
                setZoom(key)
                setSelectedId(null)
              }}
            >
              {label}
            </button>
          ))}
        </div>
      </div>

      <div
        className="tl-scroll"
        ref={scroller}
        onScroll={(e) => setScrollLeft(e.currentTarget.scrollLeft)}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={endDrag}
        onPointerCancel={endDrag}
      >
        {width > 0 && (
          <div className="tl-track" style={{ width: trackWidth }}>
            {nights.map((n) => (
              <span key={n.start} className="tl-night" style={{ left: x(n.start), width: x(n.end) - x(n.start) }} />
            ))}

            <div className="tl-band">
              {segments.map((s) => (
                <span
                  key={s.start}
                  className={s.moving ? 'tl-seg moving' : 'tl-seg'}
                  style={{
                    left: x(s.start),
                    width: Math.max(1, x(Math.min(s.end, end)) - x(s.start)),
                    // More movement in the bucket: taller and stronger purple.
                    ...(s.moving && { '--share': Math.max(0.15, s.share) }),
                  }}
                />
              ))}
            </div>

            {ticks.map((t) => (
              <span key={t} className="tl-tick" style={{ left: x(t) }}>
                {fmt.axisTick(t, visibleMs)}
              </span>
            ))}
            <span className="tl-now" style={{ left: x(end) }}>
              <span>Now</span>
            </span>

            {groups.map(({ x: left, items }) => {
              const first = items[0]
              const id = first.id
              const kind = items.length > 1 ? 'alert' : alertKind(first.code)
              const label =
                items.length > 1
                  ? `${items.length} alerts around ${fmt.dateTime(first.ts)}`
                  : `${alertTitle(first.description)}, ${fmt.dateTime(first.ts)}`
              return (
                <button
                  key={id}
                  type="button"
                  className={items.some((a) => a.ts > mountedAt) ? 'tl-pin is-new' : 'tl-pin'}
                  style={{ left, '--m': `var(--${kind})` }}
                  aria-label={label}
                  aria-expanded={selectedId === id}
                  onPointerDown={(e) => e.stopPropagation()}
                  onClick={() => setSelectedId((current) => (current === id ? null : id))}
                >
                  <AlertIcon kind={kind} />
                  {items.length > 1 && <span className="tl-pin-count">{items.length}</span>}
                </button>
              )
            })}
          </div>
        )}
        {history.loading && readings.length === 0 && <span className="skeleton tl-loading" aria-label="Loading" />}
        {!history.loading && readings.length === 0 && (
          <span className="tl-empty muted">
            {history.error ? `Couldn't load history: ${history.error.message}` : 'No readings in this period'}
          </span>
        )}
      </div>

      {selected && <AlertPopover alerts={selected.items} left={popoverLeft} onClose={() => setSelectedId(null)} />}

      <div className="tl-legend" aria-hidden="true">
        <span>
          <i className="moving" /> Moving
        </span>
        <span>
          <i /> Still
        </span>
        <span>
          <i className="night" /> Night (8 PM – 7 AM)
        </span>
        <span>
          <i className="pin" /> Alert
        </span>
        <span className="tl-hint">Drag or scroll to look back</span>
      </div>
    </section>
  )
}

export default NightTimeline
