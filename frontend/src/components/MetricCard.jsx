import { Link } from 'react-router'

// Icon paths from whisker-style-tile.html (24×24, stroked).
const ICONS = {
  temp: <path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z" />,
  weight: (
    <>
      <rect x="3" y="4" width="18" height="16" rx="4" />
      <path d="M8 10a5 5 0 0 1 8 0" />
    </>
  ),
  breath: (
    <>
      <path d="M3 8h11a3 3 0 1 0-3-3" />
      <path d="M3 12h16a3 3 0 1 1-3 3" />
      <path d="M3 16h7" />
    </>
  ),
  activity: <path d="M3 12h4l2-5 4 10 2-5h6" />,
}

// Dashboard card linking to a metric's analytics page.
// tone: token name for the accent color and icon (temp, weight, breath, activity).
// value: [[number, unit], ...] pairs, or a plain string like "Still".
// change: { value: "−0.2°", label: "since 1 hr" }, shown when present.
// sub: an optional extra line, e.g. the room temperature.
// visual: optional element shown under the value, e.g. the 3D baby on the Activity card.
function MetricCard({ to, tone, name, value, change, sub, visual }) {
  const parts = typeof value === 'string' ? [[value, '']] : value

  return (
    <Link className="metric" to={to} style={{ '--m': `var(--${tone})` }}>
      <span className="m-top">
        <span className="m-icon">
          <svg viewBox="0 0 24 24" aria-hidden="true">
            {ICONS[tone]}
          </svg>
        </span>
        <span className="m-name">{name}</span>
        <span className="m-go">
          <svg viewBox="0 0 24 24" aria-hidden="true">
            <path d="M9 6l6 6-6 6" />
          </svg>
        </span>
      </span>
      <span className="m-val">
        {parts.length
          ? parts.map(([number, unit]) => (
              <span key={unit || number}>
                {number}
                {unit && <small>{unit}</small>}
              </span>
            ))
          : '--'}
      </span>
      {visual}
      {change && (
        <span className="m-change">
          <i>{change.value}</i>
          <span>{change.label}</span>
        </span>
      )}
      {sub && <span className="m-sub">{sub}</span>}
    </Link>
  )
}

export default MetricCard
