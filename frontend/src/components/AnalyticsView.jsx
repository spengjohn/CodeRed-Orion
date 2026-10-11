import { useState } from 'react'
import { RANGES, useReadings } from '../hooks/useReadings'
import { summarize } from '../utils/series'
import BackLink from './BackLink'
import CatLoader from './CatLoader'
import MetricChart from './MetricChart'
import RangePicker from './RangePicker'

// Shared layout for the analytics pages: range picker, chart, and summary
// stats for each series. Series with excludeMoving skip readings taken while
// the child was moving when computing stats.
// tone: the dashboard card's tone; the page shares its view-transition name so
// the card expands into the page (and shrinks back on the Back link).
function AnalyticsView({ title, description, series, unit, tone, defaultRange = '24h' }) {
  const [range, setRange] = useState(defaultRange)
  const { data, loading, error } = useReadings(range)
  // Only the first load shows the loader; range changes keep the old chart up.
  const firstLoad = loading && !data.length

  return (
    <section className="page metric-page" style={{ '--m': `var(--${tone})`, viewTransitionName: `metric-${tone}` }}>
      <div className="page-head">
        <div className="title-row">
          <BackLink />
          <h1>{title}</h1>
        </div>
        <p>{description}</p>
      </div>
      {firstLoad ? (
        <CatLoader label="Loading readings…" />
      ) : (
        <>
          <RangePicker value={range} onChange={setRange} />

          {error && <p className="error">Couldn't load readings: {error.message}</p>}
          <div className="panel chart-panel">
            <MetricChart
              readings={data}
              series={series}
              rangeMs={RANGES[range].ms}
              intervalMs={RANGES[range].bucketS * 1000}
              unit={unit}
            />
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
        </>
      )}
    </section>
  )
}

export default AnalyticsView
