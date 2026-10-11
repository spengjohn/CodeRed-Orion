import react from '@vitejs/plugin-react'
import { styleText } from 'node:util'
import { defineConfig, loadEnv } from 'vite'

// Warn at startup when DEV_AUTH_TOKEN has expired; otherwise the dashboard
// just shows Disconnected. Only the payload's exp is read, never logged.
function checkTokenExpiry(token) {
  try {
    const { exp } = JSON.parse(Buffer.from(token.split('.')[1], 'base64url').toString())
    if (exp * 1000 < Date.now()) {
      console.warn(`DEV_AUTH_TOKEN expired at ${new Date(exp * 1000).toLocaleString()}; paste a fresh one.`)
    }
  } catch {
    // Not a decodable JWT: skip the check and let the backend decide.
  }
}

// One terminal line per proxied call: time, method, path, status, duration,
// and where its token came from (never the token itself).
const clock = () =>
  new Date().toLocaleTimeString('en-GB', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    fractionalSecondDigits: 3,
  })
const formatMs = (ms) => (ms < 60000 ? `${(ms / 1000).toFixed(1)} s` : `${Math.round(ms / 60000)} min`)
const statusColor = (code) => (code >= 500 ? 'red' : code >= 400 ? 'yellow' : 'green')
const AUTH_TAG = { browser: '[browser token]', dev: '[dev token]', none: '[no token]' }

function logLine(req, result) {
  console.log(`${styleText('dim', clock())}  ${req.method.padEnd(4)} ${req.url}  → ${result}  ${styleText('dim', AUTH_TAG[req.authSource] ?? '')}`)
}

// Dev-only request tweaks for the backend proxy:
// - attach DEV_AUTH_TOKEN, unless the browser sent its own (Auth0 login wins);
// - drop Origin on Socket.IO, since the backend only accepts its own origin
//   and skips the check without one (like the contract tests connect);
// - log each call unless DEV_PROXY_LOG=0. Socket.IO events inside an open
//   WebSocket aren't visible here, only the connection opening and closing.
function devProxyHooks(token, { stripOrigin = false, log = true } = {}) {
  const tweak = (proxyReq, req) => {
    req.startedAt = Date.now()
    req.authSource = req.headers.authorization ? 'browser' : token ? 'dev' : 'none'
    if (req.authSource === 'dev') proxyReq.setHeader('Authorization', `Bearer ${token}`)
    if (stripOrigin) proxyReq.removeHeader('origin')
  }
  return (proxy) => {
    proxy.on('proxyReq', tweak)
    proxy.on('proxyReqWs', (proxyReq, req, socket) => {
      tweak(proxyReq, req)
      if (!log) return
      logLine(req, styleText('cyan', 'WS open'))
      socket.once('close', () =>
        logLine(req, styleText('cyan', `WS closed after ${formatMs(Date.now() - req.startedAt)}`)),
      )
    })
    if (!log) return
    proxy.on('proxyRes', (proxyRes, req) => {
      const code = proxyRes.statusCode
      logLine(req, `${styleText(statusColor(code), String(code))}  ${Date.now() - req.startedAt} ms`)
    })
    proxy.on('error', (err, req) => {
      if (req?.method) logLine(req, styleText('red', `✖ ${err.code ?? err.message} (backend unreachable?)`))
    })
  }
}

// https://vite.dev/config/
export default defineConfig(({ command, mode }) => {
  // '' loads every variable in .env, not just VITE_ ones; these stay config-only.
  const env = loadEnv(mode, process.cwd(), '')
  const backend =
    env.BACKEND_URL || `http://${env.BACKEND_HOST || 'localhost'}:${env.BACKEND_PORT || 5000}`
  const token = env.DEV_AUTH_TOKEN
  const log = env.DEV_PROXY_LOG !== '0'
  const frontend = {
    host: env.FRONTEND_HOST || 'localhost',
    port: Number(env.FRONTEND_PORT) || 5173,
    strictPort: true,
  }

  if (command === 'serve') {
    const notes = [token && 'DEV_AUTH_TOKEN attached', log && 'logging calls'].filter(Boolean)
    console.log(`Dev proxy → ${backend}${notes.length ? ` (${notes.join(', ')})` : ''}`)
    if (token) checkTokenExpiry(token)
  }

  // changeOrigin: a remote backend behind nginx routes on the Host header.
  const target = { target: backend, changeOrigin: true, secure: true }

  return {
    plugins: [react()],
    server: {
      ...frontend,
      // Forward REST and Socket.IO calls to the Flask backend (local, or a
      // deployment via BACKEND_URL) so dev uses the same paths as nginx.
      proxy: {
        '/api': { ...target, configure: devProxyHooks(token, { log }) },
        '/socket.io': { ...target, ws: true, configure: devProxyHooks(token, { stripOrigin: true, log }) },
      },
    },
    preview: frontend,
  }
})
