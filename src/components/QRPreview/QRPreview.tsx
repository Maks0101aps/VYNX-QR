import { useState, type DragEvent, type JSX } from 'react';

import type { RenderResult } from '@/types/qr';
import styles from './QRPreview.module.css';

interface QRPreviewProps {
  result: RenderResult | null;
  busy: boolean;
  hasLogoSlot: boolean;
  onDropLogo: (file: File) => void;
}
/** Decorative mark shown before anything has been entered. */
function EmptyArt(): JSX.Element {
  return (
    <svg
      className={styles.emptyArt}
      width="132"
      height="132"
      viewBox="0 0 132 132"
      role="presentation"
      aria-hidden="true"
    >
      <rect
        x="6"
        y="6"
        width="120"
        height="120"
        rx="26"
        fill="var(--surface-alt)"
        stroke="var(--border)"
      />
      <g fill="var(--text-tertiary)">
        <rect
          x="26"
          y="26"
          width="28"
          height="28"
          rx="7"
          fill="none"
          stroke="var(--text-tertiary)"
          strokeWidth="6"
        />
        <rect x="36" y="36" width="8" height="8" rx="2" />
        <rect
          x="78"
          y="26"
          width="28"
          height="28"
          rx="7"
          fill="none"
          stroke="var(--text-tertiary)"
          strokeWidth="6"
        />
        <rect x="88" y="36" width="8" height="8" rx="2" />
        <rect
          x="26"
          y="78"
          width="28"
          height="28"
          rx="7"
          fill="none"
          stroke="var(--text-tertiary)"
          strokeWidth="6"
        />
        <rect x="36" y="88" width="8" height="8" rx="2" />
        <rect x="68" y="68" width="8" height="8" rx="2" />
        <rect x="88" y="68" width="8" height="8" rx="2" opacity="0.55" />
        <rect x="78" y="78" width="8" height="8" rx="2" opacity="0.55" />
        <rect x="98" y="88" width="8" height="8" rx="2" opacity="0.55" />
        <rect x="68" y="98" width="8" height="8" rx="2" opacity="0.55" />
        <rect x="88" y="98" width="8" height="8" rx="2" opacity="0.35" />
        <rect x="108" y="78" width="8" height="8" rx="2" opacity="0.35" />
        <rect x="108" y="98" width="8" height="8" rx="2" opacity="0.35" />
      </g>
    </svg>
  );
}

/**
 * The QR preview.
 *
 * The image is produced by the Rust renderer at 1024 px on a module aligned
 * integer grid, so it is downscaled, never upscaled, and stays perfectly crisp.
 * Dropping an image file here adds it as the logo.
 */
export function QRPreview({ result, busy, hasLogoSlot, onDropLogo }: QRPreviewProps): JSX.Element {
  const [dragOver, setDragOver] = useState(false);

  const handleDrop = (event: DragEvent<HTMLDivElement>): void => {
    event.preventDefault();
    setDragOver(false);
    if (!hasLogoSlot) return;
    const file = [...event.dataTransfer.files].find((candidate) =>
      candidate.type.startsWith('image/'),
    );
    if (file) onDropLogo(file);
  };

  if (!result) {
    return (
      <div className={styles.stage}>
        <div className={styles.empty}>
          <EmptyArt />
          <p className={styles.emptyTitle}>Create a QR code</p>
          <p className={styles.emptyBody}>
            Paste or type anything above. VYNX QR works out the type on its own and shows the code
            right away.
          </p>
          <p className={styles.privacy}>Your QR codes are generated locally.</p>
        </div>
      </div>
    );
  }

  return (
    <div className={styles.stage}>
      <div
        className={[styles.card, dragOver ? styles.dropzone : undefined].filter(Boolean).join(' ')}
        onDragOver={(event) => {
          if (!hasLogoSlot) return;
          event.preventDefault();
          setDragOver(true);
        }}
        onDragLeave={() => setDragOver(false)}
        onDrop={handleDrop}
      >
        <img
          key={result.pngBase64.length + result.version + result.ecLevel}
          className={styles.qr}
          src={`data:image/png;base64,${result.pngBase64}`}
          width={result.width}
          height={result.height}
          alt="Generated QR code"
          draggable={false}
        />
        {dragOver ? <span className={styles.dropHint}>Drop to use as logo</span> : null}
        {busy ? <span className={styles.busy}>Rendering…</span> : null}
      </div>
    </div>
  );
}
