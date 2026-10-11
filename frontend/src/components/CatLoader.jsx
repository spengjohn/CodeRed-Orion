import { createPortal } from 'react-dom'
import { CAT_COLORS } from './catColors'
import CatLogo from './CatLogo'

// The loading screen: the 3×3 grid of cat tiles pulsing in a diagonal wave.
// By default it's centered in the viewport, portaled to <body> because .page's
// page-in transform would otherwise become the containing block for the fixed
// overlay. inline: centered in its parent instead (e.g. an image slot).
function CatLoader({ size = 48, label = 'Loading…', inline = false }) {
  const grid = (
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
  )
  const status = (
    <div className={inline ? 'cat-loader-inline' : 'cat-loader-screen'} role="status">
      {grid}
      <span className="sr-only">{label}</span>
    </div>
  )

  return inline ? status : createPortal(status, document.body)
}

export default CatLoader
