import type { JSX } from 'react';

import type { RenderResult } from '@/types/qr';
import styles from './StatusBar.module.css';

const STATUS_TEXT = {
  verified: 'Scan verified',
  failed: 'QR could not be verified',
  mismatch: 'Verification mismatch',
} as const;

interface StatusBarProps {
  result: RenderResult | null;
  error: string | null;
  size: number;
}
/**
 * Honest status line. "Scan verified" only appears when the Rust decoder really
 * read the rendered image back and matched the payload.
 */
export function StatusBar({ result, error, size }: StatusBarProps): JSX.Element {
  if (error) {
    return (
      <div className={styles.wrap}>
        <span className={styles.status} data-state="failed" role="status">
          <span className={styles.dot} aria-hidden="true" />
          {error}
        </span>
      </div>
    );
  }

  if (!result) {
    return <div className={styles.wrap} />;
  }

  const { verification } = result;
  const reducedFailed = verification.reduced !== null && verification.reduced !== 'verified';

  return (
    <div className={styles.wrap}>
      <span className={styles.status} data-state={verification.status} role="status">
        <span className={styles.dot} aria-hidden="true" />
        {STATUS_TEXT[verification.status]}
        {reducedFailed ? ' at small sizes' : ''}
      </span>

      <span className={styles.meta}>
        {result.width} × {result.height}
        {result.ecAdjusted ? ' · error correction raised to High' : ''}
      </span>

      {result.warnings.length > 0 ? (
        <div className={styles.warnings}>
          {result.warnings.map((warning) => (
            <p key={`${warning.code}-${warning.message}`} className={styles.warning}>
              <span aria-hidden="true">⚠</span>
              {warning.message}
            </p>
          ))}
        </div>
      ) : null}

      <span className="visually-hidden" aria-live="polite">
        Export size {size} pixels.
      </span>
    </div>
  );
}
