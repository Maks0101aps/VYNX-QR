import type { JSX } from 'react';

import type { ReactNode } from 'react';

import styles from './ui.module.css';

interface ToggleProps {
  label: string;
  description?: string;
  checked: boolean;
  onChange: (checked: boolean) => void;
}
/** Settings switch with an accessible label and optional description. */
export function Toggle({ label, description, checked, onChange }: ToggleProps): JSX.Element {
  return (
    <div className={styles.toggleRow}>
      <span className={styles.toggleText}>
        <span className={styles.toggleTitle}>{label}</span>
        {description ? <span className={styles.toggleDescription}>{description}</span> : null}
      </span>
      <button
        type="button"
        role="switch"
        aria-checked={checked}
        aria-label={label}
        className={styles.toggle}
        onClick={() => onChange(!checked)}
      />
    </div>
  );
}

interface InfoNoticeProps {
  /** Fallback text when no children are supplied. */
  title?: string;
  children?: ReactNode;
  tone?: 'neutral' | 'warning' | 'danger' | 'success';
}
export function InfoNotice({ title, children, tone = 'neutral' }: InfoNoticeProps): JSX.Element {
  return (
    <div
      className={styles.notice}
      data-tone={tone}
      style={{
        borderColor: tone === 'neutral' ? undefined : 'var(--accent-border)',
        background:
          tone === 'warning'
            ? 'var(--warning-soft)'
            : tone === 'danger'
              ? 'var(--danger-soft)'
              : tone === 'success'
                ? 'var(--success-soft)'
                : undefined,
      }}
    >
      <span>{children ?? title}</span>
    </div>
  );
}
