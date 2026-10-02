import type { JSX } from 'react';

import type { Analysis } from '@/types/qr';
import styles from './DetectionChip.module.css';

const GLYPH: Record<Analysis['kind'], string> = {
  url: '↗',
  email: '✉',
  phone: '☎',
  text: 'Aa',
};

interface DetectionChipProps {
  analysis: Analysis | null;
  /** True when the user chose to encode the raw text instead of the URL. */
  usingOriginalText: boolean;
  onToggleOriginal: () => void;
}

/**
 * Quiet confirmation of what the detector decided, including the one case where
 * VYNX QR would change the data: adding `https://`.
 */
export function DetectionChip({
  analysis,
  usingOriginalText,
  onToggleOriginal,
}: DetectionChipProps): JSX.Element | null {
  if (!analysis) return null;

  const showsNormalization = analysis.normalization !== null && !usingOriginalText;

  return (
    <div className={styles.wrap} aria-live="polite">
      <span className={styles.chip}>
        <span className={styles.glyph} aria-hidden="true">
          {GLYPH[analysis.kind]}
        </span>
        {usingOriginalText && analysis.kind === 'url' ? 'Text' : analysis.kind.toUpperCase()}
      </span>

      {showsNormalization ? (
        <>
          <span className={styles.note}>{analysis.normalization}</span>
          <button type="button" className={styles.keepOriginal} onClick={onToggleOriginal}>
            Use original text
          </button>
        </>
      ) : null}

      {usingOriginalText && analysis.kind === 'url' ? (
        <button type="button" className={styles.keepOriginal} onClick={onToggleOriginal}>
          Use detected URL
        </button>
      ) : null}
    </div>
  );
}
