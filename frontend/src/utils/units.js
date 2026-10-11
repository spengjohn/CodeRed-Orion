// Unit conversion and display formatting. The backend always sends °C and kg;
// these convert to whatever the user picked in Settings.

const LB_PER_KG = 2.20462262
const OZ_PER_KG = 35.2739619

const isMissing = (value) => value === null || value === undefined || Number.isNaN(value)

export const cToF = (c) => (c * 9) / 5 + 32

export const kgToLb = (kg) => kg * LB_PER_KG

// Whole pounds plus whole ounces, e.g. 4.2 kg -> { lb: 9, oz: 4 }.
export function kgToLbOz(kg) {
  const totalOz = Math.round(kg * OZ_PER_KG)
  return { lb: Math.floor(totalOz / 16), oz: totalOz % 16 }
}

// Numeric conversions for charts. 'lb_oz' plots as decimal pounds.
export function convertTemp(c, unit) {
  if (isMissing(c)) return null
  return unit === 'F' ? cToF(c) : c
}

export function convertWeight(kg, unit) {
  if (isMissing(kg)) return null
  return unit === 'kg' ? kg : kgToLb(kg)
}

export const tempUnitLabel = (unit) => (unit === 'F' ? '°F' : '°C')

export const weightUnitLabel = (unit) => (unit === 'kg' ? 'kg' : 'lb')

// Display values split into [number, unit] pairs, so cards can style the
// unit smaller. Empty when the value is missing.
export function tempParts(c, unit) {
  if (isMissing(c)) return []
  return [[convertTemp(c, unit).toFixed(1), tempUnitLabel(unit)]]
}

export function weightParts(kg, unit) {
  if (isMissing(kg)) return []
  if (unit === 'lb_oz') {
    const { lb, oz } = kgToLbOz(kg)
    return [
      [String(lb), 'lb'],
      [String(oz), 'oz'],
    ]
  }
  if (unit === 'lb') return [[kgToLb(kg).toFixed(2), 'lb']]
  return [[kg.toFixed(2), 'kg']]
}

const joinParts = (parts) =>
  parts.length ? parts.map(([value, unit]) => `${value} ${unit}`).join(' ') : '--'

// Display strings for the UI.
export const formatTemp = (c, unit) => joinParts(tempParts(c, unit))

export const formatWeight = (kg, unit) => joinParts(weightParts(kg, unit))

// "+0.2", "−0.2" (true minus sign), "0.0" for no change.
function signed(n, digits) {
  const text = Math.abs(n).toFixed(digits)
  if (Number(text) === 0) return text
  return `${n > 0 ? '+' : '−'}${text}`
}

// Changes between two readings, e.g. "−0.2°", "+0.05 kg", "+3 oz", "+2".
// A temperature difference converts by the scale only (no +32 offset).
export function formatTempChange(deltaC, unit) {
  if (isMissing(deltaC)) return '--'
  return `${signed(unit === 'F' ? (deltaC * 9) / 5 : deltaC, 1)}°`
}

export function formatWeightChange(deltaKg, unit) {
  if (isMissing(deltaKg)) return '--'
  if (unit === 'lb_oz') return `${signed(Math.round(deltaKg * OZ_PER_KG), 0)} oz`
  if (unit === 'lb') return `${signed(kgToLb(deltaKg), 2)} lb`
  return `${signed(deltaKg, 2)} kg`
}

export function formatBreathingChange(delta) {
  if (isMissing(delta)) return '--'
  return signed(Math.round(delta), 0)
}

export function formatBreathing(bpm) {
  if (isMissing(bpm)) return '--'
  return `${Math.round(bpm)} breaths/min`
}
