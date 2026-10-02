import { useEffect, useRef, type JSX } from 'react';

import styles from './SmartInput.module.css';

const NEAR_LIMIT = 1800;

interface SmartInputProps {
  value: string;
  onChange: (value: string) => void;
  onClear: () => void;
  disabled?: boolean;
}
/**
 * The single universal input field.
 *
 * The user never picks a type: whatever is pasted or typed is classified by the
 * Rust detector and the QR updates on its own.
 */
export function SmartInput({
  value,
  onChange,
  onClear,
  disabled = false,
}: SmartInputProps): JSX.Element {
  const ref = useRef<HTMLTextAreaElement>(null);

  // Grow with the content up to a sensible cap, then scroll.
  useEffect(() => {
    const element = ref.current;
    if (!element) return;
    element.style.height = 'auto';
    element.style.height = `${Math.min(element.scrollHeight, 220)}px`;
  }, [value]);

  return (
    <div className={styles.wrap}>
      <label className={styles.label} htmlFor="vynx-input">
        Create QR
      </label>
      <p className={styles.subtitle} id="vynx-input-hint">
        Paste or type anything
      </p>

      <div className={styles.inputShell}>
        <textarea
          id="vynx-input"
          ref={ref}
          className={styles.input}
          value={value}
          disabled={disabled}
          spellCheck={false}
          autoComplete="off"
          aria-describedby="vynx-input-hint"
          placeholder="Start typing or paste with Ctrl + V"
          onChange={(event) => onChange(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === 'Escape' && value) {
              event.stopPropagation();
              onClear();
            }
          }}
        />
        {value ? (
          <button
            type="button"
            className={styles.clear}
            aria-label="Clear input"
            title="Clear input (Esc)"
            onClick={onClear}
          >
            <span aria-hidden="true">✕</span>
          </button>
        ) : null}
      </div>

      <span className={styles.counter} data-warning={value.length > NEAR_LIMIT} aria-live="off">
        {value.length > 600 ? `${value.length} characters` : ''}
      </span>
    </div>
  );
}
