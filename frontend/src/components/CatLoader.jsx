import { CAT_COLORS } from './catColors'
import CatLogo from './CatLogo'

// The loading screen: the 3×3 grid of cat tiles, centered in the viewport and
// pulsing in a diagonal wave.
function CatLoader({ size = 48, label = 'Loading…' }) {
  return (
    <div className="cat-loader-screen" role="status">
      <div className="cat-loader" style={{ '--tile': `${size}px` }}>
        {CAT_COLORS.map((color, i) => (
          <CatLogo
            key={color}
            color={color}
            size={size}
            // Delay by row + column, so the wave runs corner to corner.
            className="cat-wave"
            style={{ '--d': Math.floor(i / 3) + (i % 3) }}
          />
        ))}
      </div>
      <span className="sr-only">{label}</span>
    </div>
  )
}

export default CatLoader
