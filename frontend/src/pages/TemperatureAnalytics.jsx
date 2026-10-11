import AnalyticsView from '../components/AnalyticsView'
import { useFormatters } from '../hooks/useFormatters'

function TemperatureAnalytics() {
  const fmt = useFormatters()
  const series = [
    { key: 'babyTempC', name: 'Baby', color: 'temp', convert: fmt.convertTemp, format: fmt.temp },
    { key: 'ambientTempC', name: 'Room', color: 'secondary', convert: fmt.convertTemp, format: fmt.temp },
  ]

  return (
    <AnalyticsView
      title="Temperature"
      tone="temp"
      description="Baby and ambient (room) temperature over time."
      series={series}
      unit={fmt.tempUnit}
    />
  )
}

export default TemperatureAnalytics
