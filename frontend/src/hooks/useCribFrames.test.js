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
  expect(result.current).toEqual({ src: null, isLive: false })

  act(() => pushFrame({ ts: 1760112000, frame: 'AAAA', frame_id: 1 }))
  expect(result.current).toEqual({ src: 'data:image/jpeg;base64,AAAA', isLive: true })

  act(() => jest.advanceTimersByTime(6000))
  expect(result.current.isLive).toBe(false)

  unmount()
  expect(close).toHaveBeenCalled()
})

test('uses frames that are already data URLs as-is', () => {
  const { result } = renderHook(() => useCribFrames())
  act(() => pushFrame({ ts: 1760112000, frame: 'data:image/jpeg;base64,AAAA', frameId: 3 }))
  expect(result.current).toEqual({ src: 'data:image/jpeg;base64,AAAA', isLive: true })
})

test('ignores frames without image data', () => {
  const { result } = renderHook(() => useCribFrames())
  act(() => pushFrame({ ts: 1760112000, frame_id: 2 }))
  expect(result.current.src).toBeNull()
  act(() => pushFrame({ ts: 1760112000, frame: '', frameId: 4 }))
  expect(result.current).toEqual({ src: null, isLive: false })
})
