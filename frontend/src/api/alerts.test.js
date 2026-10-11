import { alertKind, alertTitle, clearAlerts, normalizeAlert } from './alerts'
import { del } from './client'

jest.mock('./client', () => ({ BASE_URL: '/api', get: jest.fn(), del: jest.fn(), getAccessToken: jest.fn() }))

test('clearAlerts sends the cutoff in seconds and returns how many were removed', async () => {
  del.mockResolvedValue({ message: 'Alerts cleared', removed: 3 })
  await expect(clearAlerts(1791712726999)).resolves.toBe(3)
  expect(del).toHaveBeenCalledWith('/alerts?before=1791712726')
})

test('normalizes a log entry from GET /alerts', () => {
  expect(
    normalizeAlert({
      id: 19,
      code: 4,
      description: "the baby's breathing rate is very low",
      ts: 1791712726,
      details: { message: 'apnea 22s' },
      hasSnapshot: true,
      snapshotUrl: '/api/alerts/19/snapshot',
    }),
  ).toEqual({
    id: 19,
    code: 4,
    description: "the baby's breathing rate is very low",
    ts: 1791712726000,
    details: { message: 'apnea 22s' },
    hasSnapshot: true,
    image: null,
  })
})

test('normalizes a live emergency_alert, using dbId so it matches the log entry', () => {
  const alert = normalizeAlert({
    id: 'alert-1791712726',
    dbId: 19,
    code: 4,
    description: "the baby's breathing rate is very low",
    ts: 1791712726,
    details: {},
    image: 'data:image/jpeg;base64,AAAA',
    type: '4',
  })
  expect(alert.id).toBe(19)
  expect(alert.hasSnapshot).toBe(true)
  expect(alert.image).toBe('data:image/jpeg;base64,AAAA')
})

test('a live alert the backend could not save still gets an id', () => {
  const alert = normalizeAlert({ dbId: null, code: null, description: '', ts: 5, details: null, image: '' })
  expect(alert.id).toBe('live-5')
  expect(alert.description).toBe('Alert from the crib')
  expect(alert.details).toEqual({})
  expect(alert.hasSnapshot).toBe(false)
})

test('groups codes by kind and turns descriptions into titles', () => {
  expect([1, 2, 3, 4, 5, 9, null].map(alertKind)).toEqual([
    'temp',
    'temp',
    'temp',
    'breath',
    'breath',
    'alert',
    'alert',
  ])
  expect(alertTitle("the baby's temperature is high")).toBe('Temperature is high')
})

test("maps the board's alert codes 101-104", () => {
  expect([101, 102, 103, 104].map(alertKind)).toEqual(['breath', 'temp', 'temp', 'weight'])
  expect(alertTitle('the baby has stopped breathing')).toBe('The baby has stopped breathing')
  expect(alertTitle("the baby's temperature is outside the safe range")).toBe(
    'Temperature is outside the safe range',
  )
  expect(alertTitle('an alert with code 9')).toBe('An alert with code 9')
})
