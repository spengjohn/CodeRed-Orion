// Icon for an alert's kind (see alertKind in api/alerts.js), in the same
// stroked 24×24 style as the metric cards.
const PATHS = {
  temp: <path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z" />,
  breath: (
    <>
      <path d="M3 8h11a3 3 0 1 0-3-3" />
      <path d="M3 12h16a3 3 0 1 1-3 3" />
      <path d="M3 16h7" />
    </>
  ),
  alert: (
    <>
      <path d="M12 4 2.5 20h19z" />
      <path d="M12 10v4M12 17.5v.01" />
    </>
  ),
}

function AlertIcon({ kind }) {
  return (
    <span className="alert-icon">
      <svg viewBox="0 0 24 24" aria-hidden="true">
        {PATHS[kind] ?? PATHS.alert}
      </svg>
    </span>
  )
}

export default AlertIcon
