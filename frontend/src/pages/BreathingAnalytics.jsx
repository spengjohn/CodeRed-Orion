import AnalyticsView from '../components/AnalyticsView'
import { useFormatters } from '../hooks/useFormatters'

function BreathingAnalytics() {
  const fmt = useFormatters()
  const series = [
    {
      key: 'breathsPerMin',
      name: 'Breathing',
      color: 'breath',
      convert: (bpm) => bpm,
      format: fmt.breathing,
    },
  ]

  return (
    <AnalyticsView
      title="Breathing"
      description="Breaths per minute over time. Shaded areas mark movement."
      series={series}
      unit="bpm"
    />
  )
}

export default BreathingAnalytics
