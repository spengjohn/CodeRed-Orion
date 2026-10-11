// Used only by Jest (babel-jest). Vite builds with its own transformer.
module.exports = {
  presets: [
    ['@babel/preset-env', { targets: { node: 'current' } }],
    ['@babel/preset-react', { runtime: 'automatic' }],
  ],
  // Jest can't parse import.meta.env; this rewrites it to process.env.
  plugins: ['babel-plugin-transform-vite-meta-env'],
}
