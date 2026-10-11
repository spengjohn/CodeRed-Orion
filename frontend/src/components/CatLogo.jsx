import { useId } from 'react'

// The cat tile logo, drawn from tokens so light and dark mode come for free.
function CatLogo({ color = 'primary', size = 32, className, style, title }) {
  // Unique per instance, since the loader renders several logos at once.
  const clipId = useId()
  return (
    <svg
      className={className ? `cat-logo ${className}` : 'cat-logo'}
      style={style}
      width={size}
      height={size}
      viewBox="0 0 100 100"
      fill="none"
      role={title ? 'img' : undefined}
      aria-label={title}
      aria-hidden={title ? undefined : true}
    >
      <g clipPath={`url(#${clipId})`}>
        <rect width="100" height="100" rx="12" fill={`var(--${color})`} />
        <g stroke="var(--onaccent)" strokeWidth="4" strokeLinejoin="round">
          <path d="M-3 33L12 10L30 28C43.3333 22 56.6667 22 70 28L88 10L103 33" />
          <path
            d="M42.864 64.152C42.508 63.6774 42.8467 63 43.44 63H55.56C56.1533 63 56.492 63.6773 56.136 64.152L50.076 72.232C49.788 72.616 49.212 72.616 48.924 72.232L42.864 64.152Z"
            fill="var(--onaccent)"
          />
          <path d="M0 59H28M0 70H28M72 59H100M72 70H100" strokeLinecap="round" />
        </g>
      </g>
      <defs>
        <clipPath id={clipId}>
          <rect width="100" height="100" rx="12" />
        </clipPath>
      </defs>
    </svg>
  )
}

export default CatLogo
