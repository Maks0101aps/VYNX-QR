import { useId, type ReactNode, type JSX } from 'react';

import styles from './ui.module.css';

interface FieldProps {
  label: string;
  hint?: string;
  error?: string;
  children: (props: { id: string; describedBy: string | undefined }) => ReactNode;
}
/**
 * Label, control and message wired together with the right ARIA relationships.
 */
export function Field({ label, hint, error, children }: FieldProps): JSX.Element {
  const id = useId();
  const hintId = hint ? `${id}-hint` : undefined;
  const errorId = error ? `${id}-error` : undefined;
  const describedBy = [errorId, hintId].filter(Boolean).join(' ') || undefined;

  return (
    <div className={styles.field}>
      <label className={styles.fieldLabel} htmlFor={id}>
        {label}
      </label>
      {children({ id, describedBy })}
      {error ? (
        <span className={styles.fieldError} id={errorId}>
          {error}
        </span>
      ) : null}
      {hint ? (
        <span className={styles.fieldHint} id={hintId}>
          {hint}
        </span>
      ) : null}
    </div>
  );
}
