import { lazy, Suspense } from 'react'
import CribVideo from '../components/CribVideo'
import MetricCard from '../components/MetricCard'
import NightTimeline from '../components/NightTimeline'
import { useFormatters } from '../hooks/useFormatters'
import { useLiveReadings } from '../hooks/useLiveReadings'
import { SNAPSHOT_INTERVAL_MS, useReadings } from '../hooks/useReadings'
import { change, currentActivity } from '../utils/series'
import { formatDuration } from '../utils/time'

// three.js is large, so the Activity card's 3D baby loads in its own chunk.
const BabyMotion = lazy(() => import('../components/BabyMotion'))

const HOUR_MS = 60 * 60 * 1000
const DAY_MS = 24 * HOUR_MS

// Label for how far back a change reaches: "since 1 hr", "this week"...
function spanLabel(ms) {
  if (ms >= 6 * DAY_MS) return 'this week'
  if (ms >= DAY_MS) return `in ${Math.round(ms / DAY_MS)} days`
  if (ms >= 55 * 60 * 1000) return 'since 1 hr'
  return `in ${formatDuration(ms)}`
}

function changeLine(result, format) {
  return result && { value: format(result.delta), label: spanLabel(result.toTs - result.fromTs) }
}

function activityLine(state) {
  if (!state) return null
  const duration = formatDuration(state.ms)
  if (state.isMoving) return { value: state.atLeast ? `${duration}+` : duration, label: 'moving' }
  if (state.atLeast) return { value: 'No movement', label: `in ${duration}` }
  return { value: duration, label: 'since moving' }
}

function Dashboard() {
  const fmt = useFormatters()
  const { readings, latest, latestStill, hasData, isStale, isConnected, error, now } =
    useLiveReadings()
  // A week of history for the weight trend; the live hook only holds an hour.
  const week = useReadings('7d')

  const problem = !isConnected
    ? hasData || isStale || error
      ? { title: 'Disconnected', detail: 'trying to reconnect' }
      : null
    : isStale
      ? {
          title: 'Sensor offline',
          detail: latest ? `last reading ${fmt.relative(latest.ts, now)}` : 'no readings yet',
        }
      : null

  return (
    <section className="page dashboard">
      <div className="t-head">
        <h1>Dashboard</h1>
        <span className="t-mode">
          {latest ? `Updated ${fmt.relative(latest.ts, now)}` : 'Waiting for readings'}
        </span>
      </div>

      {problem && (
        <div className="alert" role="status">
          {problem.title} <span>· {problem.detail}</span>
        </div>
      )}
      {error && !hasData && <p className="error">Couldn't load readings: {error.message}</p>}

      <div className="dash-main">
        <CribVideo now={now} />

        <div className="metrics">
          <MetricCard
            to="/analytics/temperature"
            tone="temp"
            name="Temp"
            value={fmt.tempParts(latest?.babyTempC)}
            change={changeLine(change(readings, 'babyTempC'), fmt.tempChange)}
            sub={latest && `Room ${fmt.temp(latest.ambientTempC)}`}
          />
          <MetricCard
            to="/analytics/activity"
            tone="activity"
            name="Activity"
            value={latest ? (latest.isMoving ? 'Moving' : 'Still') : []}
            change={activityLine(currentActivity(readings, SNAPSHOT_INTERVAL_MS))}
            visual={
              <Suspense fallback={<span className="m-visual" />}>
                <BabyMotion isMoving={Boolean(latest?.isMoving)} />
              </Suspense>
            }
          />
          <MetricCard
            to="/analytics/weight"
            tone="weight"
            name="Weight"
            value={fmt.weightParts(latestStill?.babyWeightKg)}
            change={changeLine(change(week.data, 'babyWeightKg', { excludeMoving: true }), fmt.weightChange)}
          />
          <MetricCard
            to="/analytics/breathing"
            tone="breath"
            name="Breathing"
            value={latest ? [[String(latest.breathsPerMin), '/min']] : []}
            change={changeLine(change(readings, 'breathsPerMin'), fmt.breathingChange)}
          />
        </div>
      </div>

      <NightTimeline liveReadings={readings} now={now} />
    </section>
  )
}

export default Dashboard
