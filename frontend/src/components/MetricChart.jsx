import {
  CartesianGrid,
  Line,
  LineChart,
  ReferenceArea,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'
import { useFormatters } from '../hooks/useFormatters'
import { useThemeColors } from '../hooks/useThemeColors'
import { useTimeAxis } from '../hooks/useTimeAxis'
import { axisStyle, tooltipStyle } from './chartStyle'
import { downsample, movingRanges } from '../utils/series'

const DAY_MS = 24 * 60 * 60 * 1000

// Line chart over time for one or more metrics.
// series: [{ key, name, color, convert(raw) -> plotted number, format(raw) -> string }],
// where color is a token name from tokens.css (e.g. 'temp').
// Shaded areas mark when the child was moving, on ranges up to a day (longer
// ranges use buckets so coarse that nearly all of them count as moving).
function MetricChart({ readings, series, rangeMs, intervalMs, unit, showMoving = true }) {
  const fmt = useFormatters()
  const colors = useThemeColors()
  const points = downsample(readings)

  const data = points.map((p) => {
    const point = { ts: p.ts, raw: p }
    for (const s of series) point[s.key] = s.convert(p[s.key])
    return point
  })
  const { xAxis, margin, dividers } = useTimeAxis(data, rangeMs)
  const shadeMoving = showMoving && rangeMs <= DAY_MS
  const showLegend = series.length > 1 || shadeMoving

  if (!data.length) return <p className="muted">No readings in this range.</p>

  return (
    <>
      <ResponsiveContainer width="100%" height={320}>
        <LineChart data={data} margin={margin}>
          <CartesianGrid strokeDasharray="3 3" stroke={colors.border} vertical={false} />
          <XAxis {...axisStyle(colors)} {...xAxis} />
          <YAxis
            domain={['auto', 'auto']}
            unit={unit ? ` ${unit}` : undefined}
            width={72}
            {...axisStyle(colors)}
          />
          <Tooltip
            {...tooltipStyle(colors)}
            labelFormatter={(ms) => fmt.dateTime(ms)}
            formatter={(_, name, item) => {
              const s = series.find((x) => x.name === name)
              return [s.format(item.payload.raw[s.key]), name]
            }}
          />
          {shadeMoving &&
            movingRanges(points, intervalMs).map((r) => (
              <ReferenceArea
                key={r.start}
                x1={r.start}
                x2={r.end}
                fill={colors.activity}
                fillOpacity={0.15}
              />
            ))}
          {dividers}
          {series.map((s) => (
            <Line
              key={s.key}
              dataKey={s.key}
              name={s.name}
              stroke={colors[s.color] || s.color}
              strokeWidth={2}
              type="monotone"
              dot={false}
              isAnimationActive={false}
            />
          ))}
        </LineChart>
      </ResponsiveContainer>
      {showLegend && (
        <div className="chart-legend">
          {series.length > 1 &&
            series.map((s) => (
              <span key={s.key}>
                <i className="line" style={{ background: colors[s.color] || s.color }} /> {s.name}
              </span>
            ))}
          {shadeMoving && (
            <span>
              <i className="moving" /> Moving (readings may be less accurate)
            </span>
          )}
        </div>
      )}
    </>
  )
}

export default MetricChart
