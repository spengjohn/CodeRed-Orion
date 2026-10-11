import { act, renderHook, waitFor } from '@testing-library/react'
import { getHistory } from '../api/readings'
import { connectLive } from '../api/live'
import { useLiveReadings } from './useLiveReadings'

jest.mock('../api/readings', () => ({ getHistory: jest.fn() }))
jest.mock('../api/live', () => ({ connectLive: jest.fn() }))

const MIN = 60 * 1000
const reading = (ts, isMoving = false) => ({ ts, babyWeightKg: 4.2, isMoving })

// Capture the live callbacks so tests can push readings and connection changes.
let pushLive
let setStatus
let closeLive
beforeEach(() => {
  closeLive = jest.fn()
  connectLive.mockImplementation((onReading, onStatus) => {
    pushLive = onReading
    setStatus = onStatus
    return closeLive
  })
})

afterEach(() => {
  jest.useRealTimers()
})

test('loads the last hour of minute snapshots, then appends live readings', async () => {
  getHistory.mockResolvedValue([reading(1000), reading(2000)])
  const { result, unmount } = renderHook(() => useLiveReadings())

  expect(getHistory).toHaveBeenCalledWith(3600, 60)
  await waitFor(() => expect(result.current.latest).toEqual(reading(2000)))

  act(() => pushLive(reading(3000, true)))
  expect(result.current.readings).toHaveLength(3)
  expect(result.current.latest).toEqual(reading(3000, true))
  // Weight should come from the last reading taken while still.
  expect(result.current.latestStill).toEqual(reading(2000))

  unmount()
  expect(closeLive).toHaveBeenCalled()
})

test('ignores pushed readings that are not newer than the last one', async () => {
  getHistory.mockResolvedValue([reading(1000)])
  const { result } = renderHook(() => useLiveReadings())
  await waitFor(() => expect(result.current.readings).toHaveLength(1))

  act(() => pushLive(reading(2000)))
  act(() => pushLive(reading(2000)))
  act(() => pushLive(reading(1500)))

  expect(result.current.readings.map((r) => r.ts)).toEqual([1000, 2000])
})

test('keeps live readings that arrive before history finishes loading', async () => {
  let resolveHistory
  getHistory.mockReturnValue(new Promise((resolve) => (resolveHistory = resolve)))
  const { result } = renderHook(() => useLiveReadings())

  act(() => pushLive(reading(3000)))
  await act(async () => resolveHistory([reading(1000), reading(2000)]))

  expect(result.current.readings.map((r) => r.ts)).toEqual([1000, 2000, 3000])
})

describe('stale data', () => {
  beforeEach(() => jest.useFakeTimers())

  test('is not reported before history has loaded', () => {
    getHistory.mockReturnValue(new Promise(() => {}))
    const { result } = renderHook(() => useLiveReadings())
    expect(result.current.isStale).toBe(false)
  })

  test('is reported when there are no readings at all', async () => {
    getHistory.mockResolvedValue([])
    const { result } = renderHook(() => useLiveReadings())
    await act(async () => {})
    expect(result.current.isStale).toBe(true)
  })

  test('turns on once the newest reading is over 3 minutes old', async () => {
    getHistory.mockResolvedValue([reading(Date.now() - MIN)])
    const { result } = renderHook(() => useLiveReadings())
    await act(async () => {})
    expect(result.current.isStale).toBe(false)

    // Two more minutes pass with no new snapshot: newest is now 3+ min old.
    act(() => jest.advanceTimersByTime(2 * MIN + 5000))
    expect(result.current.isStale).toBe(true)
  })

  test('re-pushing the same old reading does not clear it, a new reading does', async () => {
    const old = reading(Date.now() - 5 * MIN)
    getHistory.mockResolvedValue([old])
    const { result } = renderHook(() => useLiveReadings())
    await act(async () => {})
    expect(result.current.isStale).toBe(true)

    act(() => pushLive(old))
    expect(result.current.isStale).toBe(true)

    act(() => pushLive(reading(Date.now())))
    expect(result.current.isStale).toBe(false)
  })
})

test('isConnected follows the Socket.IO connection status', async () => {
  getHistory.mockResolvedValue([])
  const { result } = renderHook(() => useLiveReadings())
  await act(async () => {})
  expect(result.current.isConnected).toBe(false)

  act(() => setStatus(true))
  expect(result.current.isConnected).toBe(true)

  act(() => setStatus(false))
  expect(result.current.isConnected).toBe(false)
})
