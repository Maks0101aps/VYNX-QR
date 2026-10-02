import type { JSX } from 'react';

import type { Toast } from '@/hooks/useToasts';
import styles from './Toasts.module.css';

interface ToastsProps {
  toasts: Toast[];
  onDismiss: (id: number) => void;
}
/**
 * Lightweight in-app notifications. The region is announced politely so screen
 * readers hear "Saved" or "Copied" without stealing focus.
 */
export function Toasts({ toasts, onDismiss }: ToastsProps): JSX.Element | null {
  if (toasts.length === 0) return null;

  return (
    <div className={styles.stack} role="status" aria-live="polite">
      {toasts.map((toast) => (
        <div key={toast.id} className={styles.toast} data-tone={toast.tone}>
          <span aria-hidden="true">
            {toast.tone === 'success' ? '✓' : toast.tone === 'danger' ? '⚠' : 'ℹ'}
          </span>
          <span>{toast.message}</span>
          <button
            type="button"
            aria-label={`Dismiss: ${toast.message}`}
            onClick={() => onDismiss(toast.id)}
            style={{
              marginLeft: 'auto',
              border: 'none',
              background: 'transparent',
              color: 'inherit',
              cursor: 'default',
              padding: '0 2px',
            }}
          >
            ✕
          </button>
        </div>
      ))}
    </div>
  );
}
