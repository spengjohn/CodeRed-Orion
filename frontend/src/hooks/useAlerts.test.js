import { act, renderHook, waitFor } from '@testing-library/react'
import { clearAlerts, getAlerts } from '../api/alerts'
import { connectAlerts } from '../api/live'
import { mergeAlerts, useAlerts } from './useAlerts'

jest.mock('../api/alerts', () => ({ getAlerts: jest.fn(), clearAlerts: jest.fn() }))
jest.mock('../api/live', () => ({ connectAlerts: jest.fn() }))

const alert = (id, ts) => ({ id, ts, code: 4, description: 'x', details: {}, hasSnapshot: false, image: null })

test('mergeAlerts keeps newest first and skips alerts already listed', () => {
  const list = [alert(2, 200), alert(1, 100)]
  expect(mergeAlerts(list, [alert(3, 300), alert(2, 200)]).map((a) => a.id)).toEqual([3, 2, 1])
  expect(mergeAlerts([], [alert(1, 100), alert(2, 200), alert(3, 300)], 2).map((a) => a.id)).toEqual([3, 2])
})

test('loads the log, then adds live alerts without duplicating ones already loaded', async () => {
  let pushAlert
  const close = jest.fn()
  connectAlerts.mockImplementation((onAlert) => {
    pushAlert = onAlert
    return close
  })
  getAlerts.mockResolvedValue([alert(2, 200), alert(1, 100)])

  const { result, unmount } = renderHook(() => useAlerts())
  await waitFor(() => expect(result.current.loading).toBe(false))
  expect(result.current.alerts.map((a) => a.id)).toEqual([2, 1])

  act(() => pushAlert(alert(3, 300)))
  act(() => pushAlert(alert(3, 300))) // the same alert again
  expect(result.current.alerts.map((a) => a.id)).toEqual([3, 2, 1])

  unmount()
  expect(close).toHaveBeenCalled()
})

test('clear deletes up to the newest listed alert and keeps ones that arrive meanwhile', async () => {
  let pushAlert
  connectAlerts.mockImplementation((onAlert) => {
    pushAlert = onAlert
    return () => {}
  })
  getAlerts.mockResolvedValue([alert(2, 200), alert(1, 100)])
  let finishDelete
  clearAlerts.mockImplementation(() => new Promise((resolve) => (finishDelete = resolve)))

  const { result } = renderHook(() => useAlerts())
  await waitFor(() => expect(result.current.loading).toBe(false))

  let cleared
  act(() => {
    cleared = result.current.clear()
  })
  expect(clearAlerts).toHaveBeenCalledWith(200)
  act(() => pushAlert(alert(3, 300))) // arrives while the delete is in flight
  await act(async () => {
    finishDelete(2)
    await cleared
  })
  expect(result.current.alerts.map((a) => a.id)).toEqual([3])
})

test('clear passes on a failed delete and keeps the list', async () => {
  connectAlerts.mockReturnValue(() => {})
  getAlerts.mockResolvedValue([alert(1, 100)])
  clearAlerts.mockRejectedValue(new Error('The server ran into a problem.'))

  const { result } = renderHook(() => useAlerts())
  await waitFor(() => expect(result.current.loading).toBe(false))
  await expect(act(() => result.current.clear())).rejects.toThrow('The server ran into a problem.')
  expect(result.current.alerts.map((a) => a.id)).toEqual([1])
})

test('reports a failed load', async () => {
  connectAlerts.mockReturnValue(() => {})
  getAlerts.mockRejectedValue(new Error('GET /alerts failed: 503'))
  const { result } = renderHook(() => useAlerts())
  await waitFor(() => expect(result.current.loading).toBe(false))
  expect(result.current.error.message).toBe('GET /alerts failed: 503')
})
