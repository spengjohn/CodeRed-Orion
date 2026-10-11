import { formatAxisTick, formatDate, formatDuration, formatRelative, formatTime } from './time'

// 2025-10-10 16:00:00 UTC
const MS = 1760112000 * 1000
const chicago = { timeZone: 'America/Chicago' }
const tokyo = { timeZone: 'Asia/Tokyo' }

describe('formatTime', () => {
  test('converts to the chosen time zone (12h)', () => {
    expect(formatTime(MS, { ...chicago, timeFormat: '12h' })).toMatch(/^11:00\sAM$/)
    expect(formatTime(MS, { ...tokyo, timeFormat: '12h' })).toMatch(/^1:00\sAM$/)
  })

  test('uses 24-hour time when chosen', () => {
    expect(formatTime(MS, { ...chicago, timeFormat: '24h' })).toBe('11:00')
    expect(formatTime(MS, { ...tokyo, timeFormat: '24h' })).toMatch(/^0?1:00$/)
  })
})

test('formatDate uses the local date in the chosen time zone', () => {
  expect(formatDate(MS, chicago)).toBe('Oct 10')
  expect(formatDate(MS, tokyo)).toBe('Oct 11')
})

test('formatAxisTick shows hours for short ranges and M/D for long ones', () => {
  const HOUR = 60 * 60 * 1000
  const h24 = { ...chicago, timeFormat: '24h' }
  const h12 = { ...chicago, timeFormat: '12h' }
  expect(formatAxisTick(MS, 24 * HOUR, h24)).toBe('11')
  expect(formatAxisTick(MS, 24 * HOUR, { ...tokyo, timeFormat: '24h' })).toBe('1')
  expect(formatAxisTick(MS, 24 * HOUR, h12)).toMatch(/^11\sAM$/)
  expect(formatAxisTick(MS, 24 * HOUR, h12, { meridiem: false })).toBe('11')
  expect(formatAxisTick(MS, HOUR, h24)).toBe('11:00')
  expect(formatAxisTick(MS, HOUR, h12, { meridiem: false })).toBe('11:00')
  expect(formatAxisTick(MS, 7 * 24 * HOUR, h24)).toBe('10/10')
})

test('formatRelative describes how long ago a time was', () => {
  const now = MS
  expect(formatRelative(now - 5 * 1000, now)).toBe('just now')
  expect(formatRelative(now - 2 * 60 * 1000, now)).toBe('2 minutes ago')
  expect(formatRelative(now - 3 * 60 * 60 * 1000, now)).toBe('3 hours ago')
  expect(formatRelative(now - 24 * 60 * 60 * 1000, now)).toBe('yesterday')
})

test('formatDuration shows seconds, minutes, or hours and minutes', () => {
  const MIN = 60 * 1000
  expect(formatDuration(45 * 1000)).toBe('45 s')
  expect(formatDuration(12 * MIN)).toBe('12 min')
  expect(formatDuration(65 * MIN)).toBe('1 h 5 min')
  expect(formatDuration(59.6 * MIN)).toBe('1 h 0 min')
})
