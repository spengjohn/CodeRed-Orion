import { act, renderHook } from '@testing-library/react'
import { connectFrames } from '../api/live'
import { useCribFrames } from './useCribFrames'

jest.mock('../api/live', () => ({ connectFrames: jest.fn() }))

let pushFrame
let close
beforeEach(() => {
  jest.useFakeTimers()
  close = jest.fn()
  connectFrames.mockImplementation((onFrame) => {
    pushFrame = onFrame
    return close
  })
})

afterEach(() => jest.useRealTimers())

test('shows the newest frame while frames keep arriving, then goes offline', () => {
  const { result, unmount } = renderHook(() => useCribFrames())
  expect(result.current).toEqual({ src: null, isLive: false, isOffline: false })

  act(() => pushFrame({ ts: 1760112000, frame: 'AAAA', frame_id: 1 }))
  expect(result.current).toEqual({
    src: 'data:image/jpeg;base64,AAAA',
    isLive: true,
    isOffline: false,
  })

  act(() => jest.advanceTimersByTime(6000))
  expect(result.current.isLive).toBe(false)
  expect(result.current.isOffline).toBe(true)

  unmount()
  expect(close).toHaveBeenCalled()
})

test('uses frames that are already data URLs as-is', () => {
  const { result } = renderHook(() => useCribFrames())
  act(() => pushFrame({ ts: 1760112000, frame: 'data:image/jpeg;base64,AAAA', frameId: 3 }))
  expect(result.current).toEqual({
    src: 'data:image/jpeg;base64,AAAA',
    isLive: true,
    isOffline: false,
  })
})

test('ignores frames without image data', () => {
  const { result } = renderHook(() => useCribFrames())
  act(() => pushFrame({ ts: 1760112000, frame_id: 2 }))
  expect(result.current.src).toBeNull()
  act(() => pushFrame({ ts: 1760112000, frame: '', frameId: 4 }))
  expect(result.current).toEqual({ src: null, isLive: false, isOffline: false })
})

test('counts as offline only after no frame arrives for 5 s from mount', () => {
  const { result } = renderHook(() => useCribFrames())
  act(() => jest.advanceTimersByTime(3000))
  expect(result.current.isOffline).toBe(false)
  act(() => jest.advanceTimersByTime(3000))
  expect(result.current.isOffline).toBe(true)

  act(() => pushFrame({ ts: 1760112000, frame: 'AAAA', frameId: 5 }))
  expect(result.current).toMatchObject({ isLive: true, isOffline: false })
})
