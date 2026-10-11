import { ReferenceLine } from 'recharts'
import { useFormatters } from './useFormatters'
import { useThemeColors } from './useThemeColors'
import { axisTicks, chartTickStep, meridiemBoundaries } from '../utils/timeline'

const DAY_MS = 24 * 60 * 60 * 1000

// Time X axis for the analytics charts: round, evenly spaced ticks for the
// range. With 12-hour time, only the first tick says AM/PM; dashed lines mark
// where it switches. points: [{ ts }].
// Usage: <Chart margin={margin}>, <XAxis {...xAxis} />, and {dividers} as
// children of the chart.
export function useTimeAxis(points, rangeMs) {
  const fmt = useFormatters()
  const colors = useThemeColors()

  const times = points.map((p) => p.ts)
  const start = Math.min(...times)
  const end = Math.max(...times)
  const ticks = points.length ? axisTicks(start, end, chartTickStep(rangeMs)) : []
  const showDividers = fmt.timeFormat === '12h' && rangeMs <= DAY_MS
  const boundaries = showDividers && points.length ? meridiemBoundaries(start, end) : []

  const xAxis = {
    dataKey: 'ts',
    type: 'number',
    scale: 'time',
    domain: ['dataMin', 'dataMax'],
    ticks,
    minTickGap: 32,
    tickLine: { stroke: colors.border },
    tickSize: 6,
    tickFormatter: (ms, i) => fmt.axisTick(ms, rangeMs, { meridiem: !showDividers || i === 0 }),
  }

  // The AM/PM labels sit above the plot, so leave room for them.
  const margin = {
    top: boundaries.length ? 24 : 8,
    right: 16,
    bottom: 8,
    left: 0,
  }

  const dividers = boundaries.map(({ ts, period }) => (
    <ReferenceLine
      key={ts}
      x={ts}
      stroke={colors.muted}
      strokeDasharray="4 4"
      label={{
        value: period,
        position: 'top',
        fill: colors.muted,
        fontSize: 11,
      }}
    />
  ))

  return { xAxis, margin, dividers }
}
