import { io } from 'socket.io-client'
import { API_URL, AUTH_TOKEN, requireBackend } from './backend.js'
import { expectValidReading } from './packetSchema.js'

// Supertest doesn't speak Socket.IO, so this uses socket.io-client directly.
beforeAll(requireBackend)

const connect = (auth) => io(API_URL, { transports: ['websocket'], reconnection: false, auth })

test('Socket.IO refuses connections without an Auth0 token', async () => {
  const socket = connect()
  try {
    await new Promise((resolve, reject) => {
      socket.once('connect_error', resolve)
      socket.once('connect', () => reject(new Error('connected without a token')))
    })
  } finally {
    socket.disconnect()
  }
}, 5000)

// Needs AUTH_TOKEN (see backend.js); skipped without one.
const withToken = AUTH_TOKEN ? test : test.skip

withToken('Socket.IO "vitals" event pushes valid readings', async () => {
  const socket = connect({ token: AUTH_TOKEN })
  try {
    const reading = await new Promise((resolve, reject) => {
      socket.once('vitals', resolve)
      socket.once('connect_error', reject)
    })
    expectValidReading(reading)
  } finally {
    socket.disconnect()
  }
}, 5000)
