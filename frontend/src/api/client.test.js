import { get, setAccessTokenGetter } from './client'

const mockFetch = (status = 200) => {
  globalThis.fetch = jest.fn().mockResolvedValue({ ok: status < 400, status, json: async () => ({}) })
}
const sentHeaders = () => fetch.mock.calls[0][1].headers

afterEach(() => setAccessTokenGetter(null))

test('sends the access token as a Bearer header when signed in', async () => {
  mockFetch()
  setAccessTokenGetter(async () => 'abc')
  await get('/readings/latest')
  expect(sentHeaders()).toHaveProperty('Authorization', 'Bearer abc')
})

test('sends no Authorization header without a token', async () => {
  mockFetch()
  await get('/readings/latest')
  expect(sentHeaders()).not.toHaveProperty('Authorization')
})

test('treats a failing token getter as no token', async () => {
  mockFetch()
  setAccessTokenGetter(async () => {
    throw new Error('login_required')
  })
  await get('/readings/latest')
  expect(sentHeaders()).not.toHaveProperty('Authorization')
})

test('explains a 401 and keeps the status', async () => {
  mockFetch(401)
  await expect(get('/readings/latest')).rejects.toMatchObject({
    status: 401,
    message: expect.stringMatching(/sign in again/i),
  })
})
