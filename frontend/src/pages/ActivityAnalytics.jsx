import { useState } from 'react'
import ActivityChart from '../components/ActivityChart'
import BackLink from '../components/BackLink'
import RangePicker from '../components/RangePicker'
import { useActivity } from '../hooks/useActivity'
import { RANGES } from '../hooks/useReadings'
import { formatDuration } from '../utils/time'

function ActivityAnalytics() {
  const [range, setRange] = useState('24h')
  const { series, stats, loading, error } = useActivity(range)

  return (
    // Shares the dashboard card's view-transition name, so the card expands into this page.
    <section
      className="page metric-page"
      style={{ '--m': 'var(--activity)', viewTransitionName: 'metric-activity' }}
    >
      <div className="page-head">
        <div className="title-row">
          <BackLink />
          <h1>Activity</h1>
        </div>
        <p>
          When the child was moving. The crib reports once a minute; each step is a time period
          marked moving if any minute in it had movement, so longer ranges (with longer periods)
          overstate time spent moving.
        </p>
      </div>
      <RangePicker value={range} onChange={setRange} />

      {error && <p className="error">Couldn't load readings: {error.message}</p>}
      <div className="panel chart-panel">
        {loading && !series.length ? (
          <p className="muted">Loading…</p>
        ) : (
          <ActivityChart points={series} rangeMs={RANGES[range].ms} />
        )}
      </div>

      <div className="stats">
        <dl className="stat-group">
          <dt>Movement episodes</dt>
          <dd>{stats.episodes}</dd>
        </dl>
        <dl className="stat-group">
          <dt>Total time moving</dt>
          <dd>{formatDuration(stats.totalMs)}</dd>
        </dl>
        <dl className="stat-group">
          <dt>Longest episode</dt>
          <dd>{formatDuration(stats.longestMs)}</dd>
        </dl>
        <dl className="stat-group">
          <dt>Time moving</dt>
          <dd>{stats.percent === null ? '--' : `${Math.round(stats.percent)}%`}</dd>
        </dl>
      </div>
    </section>
  )
}

export default ActivityAnalytics
