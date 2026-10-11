import { useMemo } from 'react'
import { usePreferences } from '../preferences/context'
import {
  convertTemp,
  convertWeight,
  formatBreathing,
  formatBreathingChange,
  formatTemp,
  formatTempChange,
  formatWeight,
  formatWeightChange,
  tempParts,
  tempUnitLabel,
  weightParts,
  weightUnitLabel,
} from '../utils/units'
import { formatAxisTick, formatDateTime, formatRelative, formatTime } from '../utils/time'

// Unit and time formatters bound to the user's preferences, e.g.
// const fmt = useFormatters(); fmt.temp(36.8) -> "98.2 °F"
export function useFormatters() {
  const { preferences: prefs } = usePreferences()

  return useMemo(() => {
    // Times always use the device's own zone, so a timeZone saved by older
    // versions of the settings page is ignored.
    const timePrefs = { timeFormat: prefs.timeFormat }
    return {
      temp: (c) => formatTemp(c, prefs.tempUnit),
      weight: (kg) => formatWeight(kg, prefs.weightUnit),
      breathing: formatBreathing,
      tempParts: (c) => tempParts(c, prefs.tempUnit),
      weightParts: (kg) => weightParts(kg, prefs.weightUnit),
      tempChange: (deltaC) => formatTempChange(deltaC, prefs.tempUnit),
      weightChange: (deltaKg) => formatWeightChange(deltaKg, prefs.weightUnit),
      breathingChange: formatBreathingChange,
      convertTemp: (c) => convertTemp(c, prefs.tempUnit),
      convertWeight: (kg) => convertWeight(kg, prefs.weightUnit),
      tempUnit: tempUnitLabel(prefs.tempUnit),
      weightUnit: weightUnitLabel(prefs.weightUnit),
      time: (ms) => formatTime(ms, timePrefs),
      dateTime: (ms) => formatDateTime(ms, timePrefs),
      axisTick: (ms, rangeMs) => formatAxisTick(ms, rangeMs, timePrefs),
      relative: formatRelative,
    }
  }, [prefs])
}
