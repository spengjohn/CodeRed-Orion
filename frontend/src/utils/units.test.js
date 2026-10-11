import {
  cToF,
  convertTemp,
  convertWeight,
  formatBreathing,
  formatBreathingChange,
  formatTemp,
  formatTempChange,
  formatWeight,
  formatWeightChange,
  kgToLb,
  kgToLbOz,
  tempParts,
  weightParts,
} from './units'

describe('temperature', () => {
  test('converts Celsius to Fahrenheit', () => {
    expect(cToF(37)).toBeCloseTo(98.6)
    expect(cToF(0)).toBe(32)
  })

  test('formats in the chosen unit', () => {
    expect(formatTemp(37, 'F')).toBe('98.6 °F')
    expect(formatTemp(36.8, 'C')).toBe('36.8 °C')
  })
})

describe('weight', () => {
  test('converts kilograms to pounds', () => {
    expect(kgToLb(4.2)).toBeCloseTo(9.259, 3)
  })

  test('converts kilograms to pounds and ounces', () => {
    expect(kgToLbOz(4.2)).toEqual({ lb: 9, oz: 4 })
  })

  test('ounces that round to 16 roll over into the next pound', () => {
    // 15.6 oz rounds to 16 oz, which is 1 lb 0 oz rather than 0 lb 16 oz.
    expect(kgToLbOz(15.6 / 35.2739619)).toEqual({ lb: 1, oz: 0 })
  })

  test('formats in the chosen unit', () => {
    expect(formatWeight(4.2, 'kg')).toBe('4.20 kg')
    expect(formatWeight(4.2, 'lb')).toBe('9.26 lb')
    expect(formatWeight(4.2, 'lb_oz')).toBe('9 lb 4 oz')
  })

  test('charts lb_oz as decimal pounds', () => {
    expect(convertWeight(4.2, 'lb_oz')).toBeCloseTo(9.259, 3)
  })
})

describe('missing values', () => {
  test.each([null, undefined, NaN])('%p formats as -- and converts to null', (value) => {
    expect(formatTemp(value, 'F')).toBe('--')
    expect(formatWeight(value, 'lb_oz')).toBe('--')
    expect(formatBreathing(value)).toBe('--')
    expect(convertTemp(value, 'F')).toBeNull()
    expect(convertWeight(value, 'lb')).toBeNull()
  })
})

test('formats breathing rate as whole breaths per minute', () => {
  expect(formatBreathing(38.4)).toBe('38 breaths/min')
})

describe('value parts', () => {
  test('split values from units for display', () => {
    expect(tempParts(37, 'F')).toEqual([['98.6', '°F']])
    expect(weightParts(4.2, 'kg')).toEqual([['4.20', 'kg']])
    expect(weightParts(4.2, 'lb_oz')).toEqual([
      ['9', 'lb'],
      ['4', 'oz'],
    ])
  })

  test('are empty when the value is missing', () => {
    expect(tempParts(null, 'C')).toEqual([])
    expect(weightParts(undefined, 'kg')).toEqual([])
    expect(formatTemp(null, 'C')).toBe('--')
  })
})

describe('changes', () => {
  test('temperature changes convert by scale only, with a true minus sign', () => {
    expect(formatTempChange(-0.2, 'C')).toBe('−0.2°')
    expect(formatTempChange(0.5, 'F')).toBe('+0.9°')
    expect(formatTempChange(0.01, 'C')).toBe('0.0°')
  })

  test('weight changes use the chosen unit', () => {
    expect(formatWeightChange(0.05, 'kg')).toBe('+0.05 kg')
    expect(formatWeightChange(0.1, 'lb')).toBe('+0.22 lb')
    expect(formatWeightChange(-0.1, 'lb_oz')).toBe('−4 oz')
  })

  test('breathing changes are whole breaths', () => {
    expect(formatBreathingChange(2.4)).toBe('+2')
    expect(formatBreathingChange(null)).toBe('--')
  })
})
