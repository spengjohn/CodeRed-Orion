import {
  Area,
  AreaChart,
  CartesianGrid,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'
import { useFormatters } from '../hooks/useFormatters'
import { useThemeColors } from '../hooks/useThemeColors'
import { useTimeAxis } from '../hooks/useTimeAxis'
import { axisStyle, tooltipStyle } from './chartStyle'

// Step chart of when the child was moving. points: [{ ts, movingPct }]
// from activitySeries().
function ActivityChart({ points, rangeMs }) {
  const fmt = useFormatters()
  const colors = useThemeColors()
  const { xAxis, margin, dividers } = useTimeAxis(points, rangeMs)

  if (!points.length) return <p className="muted">No readings in this range.</p>

  return (
    <ResponsiveContainer width="100%" height={320}>
      <AreaChart data={points} margin={margin}>
        <CartesianGrid strokeDasharray="3 3" stroke={colors.border} vertical={false} />
        <XAxis {...axisStyle(colors)} {...xAxis} />
        <YAxis domain={[0, 100]} ticks={[0, 50, 100]} unit="%" width={56} {...axisStyle(colors)} />
        <Tooltip
          {...tooltipStyle(colors)}
          labelFormatter={(ms) => fmt.dateTime(ms)}
          formatter={(value) => [`${Math.round(value)}%`, 'Moving']}
        />
        {dividers}
        <Area
          dataKey="movingPct"
          type="stepAfter"
          stroke={colors.activity}
          strokeWidth={2}
          fill={colors.activity}
          fillOpacity={0.3}
          isAnimationActive={false}
        />
      </AreaChart>
    </ResponsiveContainer>
  )
}

export default ActivityChart
