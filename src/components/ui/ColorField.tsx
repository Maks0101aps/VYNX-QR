import { useId, type JSX } from 'react';

import { normaliseHex } from '@/lib/color';
import styles from './ui.module.css';

interface ColorFieldProps {
  label: string;
  value: string;
  onChange: (value: string) => void;
  /** Converted to the native colour input, which only understands 6 digits. */
  pickerValue: string;
}

/**
 * Colour swatch plus a HEX text field. Invalid HEX is reported inline but never
 * blocks editing: the last valid colour stays in effect.
 */
export function ColorField({ label, value, onChange, pickerValue }: ColorFieldProps): JSX.Element {
  const id = useId();
  const errorId = `${id}-error`;
  const invalid = normaliseHex(value) === null;

  return (
    <div className={styles.field}>
      <span className={styles.fieldLabel} id={`${id}-label`}>
        {label}
      </span>
      <div className={styles.colorRow}>
        <span className={styles.swatch} style={{ backgroundColor: pickerValue }}>
          <label className="visually-hidden" htmlFor={id}>
            {label}
          </label>
          <input
            id={id}
            type="color"
            value={pickerValue}
            aria-describedby={invalid ? errorId : undefined}
            onChange={(event) => onChange(event.target.value.toUpperCase())}
          />
        </span>
        <input
          className={[styles.input, styles.hex].join(' ')}
          value={value}
          spellCheck={false}
          autoComplete="off"
          aria-label={`${label} HEX value`}
          aria-invalid={invalid}
          aria-describedby={invalid ? errorId : undefined}
          onChange={(event) => {
            const next = event.target.value;
            const normalised = normaliseHex(next);
            if (normalised) onChange(normalised);
            else onChange(next);
          }}
          onBlur={() => {
            const normalised = normaliseHex(value);
            if (normalised) onChange(normalised);
          }}
        />
      </div>
      {invalid ? (
        <span className={styles.fieldError} id={errorId}>
          Use a HEX colour such as #1A1A1C.
        </span>
      ) : null}
    </div>
  );
}
