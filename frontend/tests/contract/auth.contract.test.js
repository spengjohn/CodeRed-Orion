import { api, requireBackend } from './backend.js'

// Auth0 routes (backend/app/api/auth_routes.py). The frontend doesn't call
// them yet; these checks catch the routes going missing from create_app().
beforeAll(requireBackend)

test('GET /api/public is open', async () => {
  const res = await api().get('/api/public')
  expect(res.status).toBe(200)
})

test('GET /api/private-crib-data rejects requests without a token', async () => {
  const res = await api().get('/api/private-crib-data')
  expect(res.status).toBe(401)
  expect(res.body).toHaveProperty('error')
})
