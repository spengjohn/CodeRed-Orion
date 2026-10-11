// Shared axis and tooltip styling for the charts, from the theme colors.
export const axisStyle = (colors) => ({
  stroke: colors.border,
  tick: { fill: colors.muted, fontSize: 12 },
  tickLine: false,
})

export const tooltipStyle = (colors) => ({
  contentStyle: { background: colors.surface, borderColor: colors.border, color: colors.text },
  labelStyle: { color: colors.muted },
})
