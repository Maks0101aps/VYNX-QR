import { useRef, type JSX } from 'react';

import mark from '../../../src-tauri/icons/32x32.png';
import styles from './TitleBar.module.css';

interface TitleBarProps {
  onOpenSettings: () => void;
}
/**
 * Custom header drawn inside the native caption area.
 *
 * `titleBarStyle: "Overlay"` keeps the real Windows minimise/maximise/close
 * buttons (so Snap Layouts keep working) while the app draws its own header next
 * to them. The wide right padding reserves room for those caption buttons.
 */
export function TitleBar({ onOpenSettings }: TitleBarProps): JSX.Element {
  const settingsRef = useRef<HTMLButtonElement>(null);

  return (
    <header className={styles.titlebar} data-tauri-drag-region>
      <div className={styles.brand} data-tauri-drag-region>
        <img className={styles.mark} src={mark} alt="" width={20} height={20} />
        <span className={styles.title} data-tauri-drag-region>
          VYNX QR
        </span>
      </div>

      <div className={styles.spacer} data-tauri-drag-region />

      <div className={styles.actions}>
        <button
          ref={settingsRef}
          type="button"
          className={styles.gear}
          title="Settings"
          aria-label="Settings"
          onClick={onOpenSettings}
        >
          <span aria-hidden="true">⚙</span>
        </button>
      </div>
    </header>
  );
}
