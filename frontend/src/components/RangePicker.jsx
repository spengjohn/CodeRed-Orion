import { RANGES } from '../hooks/useReadings'

function RangePicker({ value, onChange }) {
  return (
    <div className="range-picker" role="group" aria-label="Time range">
      {Object.keys(RANGES).map((range) => (
        <button
          key={range}
          type="button"
          aria-pressed={value === range}
          onClick={() => onChange(range)}
        >
          {range}
        </button>
      ))}
    </div>
  )
}

export default RangePicker
