import { api, requireBackend } from './backend.js'

// Video relay routes (backend/app/api/video.py). Frames themselves arrive as
// the Socket.IO "video_frame" event, which needs a running edge device.
beforeAll(requireBackend)

test('GET /api/video/status reports the relay state', async () => {
  const res = await api().get('/api/video/status')
  expect(res.status).toBe(200)
  expect(typeof res.body.streaming_active).toBe('boolean')
  expect(typeof res.body.target_fps).toBe('number')
})

test('POST /api/video/stop rejects requests without a token', async () => {
  const res = await api().post('/api/video/stop')
  expect(res.status).toBe(401)
})
