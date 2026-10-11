import AnalyticsView from '../components/AnalyticsView'
import { useFormatters } from '../hooks/useFormatters'

function WeightAnalytics() {
  const fmt = useFormatters()
  const series = [
    {
      key: 'babyWeightKg',
      name: 'Weight',
      color: 'weight',
      convert: fmt.convertWeight,
      format: fmt.weight,
      excludeMoving: true,
    },
  ]

  return (
    <AnalyticsView
      title="Weight"
      tone="weight"
      description="Baby's weight over time. Shaded areas mark movement, when readings are less reliable; stats ignore them."
      series={series}
      unit={fmt.weightUnit}
      defaultRange="7d"
    />
  )
}

export default WeightAnalytics
