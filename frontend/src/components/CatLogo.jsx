// The cat tile logo, drawn from tokens so light and dark mode come for free.
function CatLogo({ color = 'primary', size = 32, className, style, title }) {
  return (
    <svg
      className={className ? `cat-logo ${className}` : 'cat-logo'}
      style={style}
      width={size}
      height={size}
      viewBox="0 0 100 100"
      role={title ? 'img' : undefined}
      aria-label={title}
      aria-hidden={title ? undefined : true}
    >
      <rect width="100" height="100" rx="12" fill={`var(--${color})`} />
      <g stroke="var(--onaccent)" strokeWidth="4" fill="none">
        <path d="M62 39L70.5 22L79 39" />
        <path d="M20 39L28.5 22L37 39" />
        <path d="M15 58H35M15 69H35M65 58H85M65 69H85" />
      </g>
      <path d="M44 59H56L50 67L44 59Z" fill="var(--onaccent)" />
    </svg>
  )
}

export default CatLogo
