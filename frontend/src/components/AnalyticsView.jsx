import { useState } from 'react'
import { RANGES, useReadings } from '../hooks/useReadings'
import { summarize } from '../utils/series'
import BackLink from './BackLink'
import MetricChart from './MetricChart'
import RangePicker from './RangePicker'

// Shared layout for the analytics pages: range picker, chart, and summary
// stats for each series. Series with excludeMoving skip readings taken while
// the child was moving when computing stats.
function AnalyticsView({ title, description, series, unit, defaultRange = '24h' }) {
  const [range, setRange] = useState(defaultRange)
  const { data, loading, error } = useReadings(range)

  return (
    <section className="page">
      <div className="page-head">
        <div className="title-row">
          <BackLink />
          <h1>{title}</h1>
        </div>
        <p>{description}</p>
      </div>
      <RangePicker value={range} onChange={setRange} />

      {error && <p className="error">Couldn't load readings: {error.message}</p>}
      <div className="panel chart-panel">
        {loading && !data.length ? (
          <p className="muted">Loading…</p>
        ) : (
          <MetricChart
            readings={data}
            series={series}
            rangeMs={RANGES[range].ms}
            intervalMs={RANGES[range].bucketS * 1000}
            unit={unit}
          />
        )}
      </div>

      <div className="stats">
        {series.map((s) => {
          const stats = summarize(data, s.key, { excludeMoving: s.excludeMoving })
          return (
            <dl key={s.key} className="stat-group">
              <dt>{s.name}</dt>
              <dd>
                <span>Min</span> {s.format(stats.min)}
              </dd>
              <dd>
                <span>Avg</span> {s.format(stats.avg)}
              </dd>
              <dd>
                <span>Max</span> {s.format(stats.max)}
              </dd>
            </dl>
          )
        })}
      </div>
    </section>
  )
}

export default AnalyticsView
