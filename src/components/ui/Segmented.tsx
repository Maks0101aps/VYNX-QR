import { useId, type JSX } from 'react';

import styles from './ui.module.css';

export interface SegmentOption<T extends string> {
  value: T;
  label: string;
  title?: string;
}

interface SegmentedProps<T extends string> {
  label: string;
  options: readonly SegmentOption<T>[];
  value: T;
  onChange: (value: T) => void;
  fullWidth?: boolean;
}

/**
 * Radio group styled as a Fluent segmented control. Implemented with real radio
 * inputs so keyboard arrow navigation and screen readers work for free.
 */
export function Segmented<T extends string>({
  label,
  options,
  value,
  onChange,
  fullWidth = false,
}: SegmentedProps<T>): JSX.Element {
  const name = useId();

  return (
    <div
      className={[styles.segmented, fullWidth ? styles.segmentedFull : undefined]
        .filter(Boolean)
        .join(' ')}
      role="radiogroup"
      aria-label={label}
    >
      {options.map((option) => (
        <label key={option.value} className={styles.segment} title={option.title ?? option.label}>
          <input
            className="visually-hidden"
            type="radio"
            name={name}
            value={option.value}
            checked={value === option.value}
            onChange={() => onChange(option.value)}
          />
          {option.label}
        </label>
      ))}
    </div>
  );
}
