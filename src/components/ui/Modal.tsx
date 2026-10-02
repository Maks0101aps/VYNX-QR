import { useEffect, useRef, type ReactNode, type JSX } from 'react';
import { createPortal } from 'react-dom';

import { Button } from './Button';
import styles from './ui.module.css';

interface ModalProps {
  title: string;
  onClose: () => void;
  children: ReactNode;
  footer?: ReactNode;
}
const FOCUSABLE =
  'button:not([disabled]), [href], input:not([disabled]), select:not([disabled]), textarea:not([disabled]), [tabindex]:not([tabindex="-1"])';

/**
 * Centred modal dialog with a focus trap, Escape handling and focus restore.
 */
export function Modal({ title, onClose, children, footer }: ModalProps): JSX.Element {
  const surfaceRef = useRef<HTMLDivElement>(null);
  const restoreRef = useRef<HTMLElement | null>(null);

  useEffect(() => {
    restoreRef.current = document.activeElement as HTMLElement | null;
    const first = surfaceRef.current?.querySelector<HTMLElement>(FOCUSABLE);
    (first ?? surfaceRef.current)?.focus();
    return () => restoreRef.current?.focus();
  }, []);

  useEffect(() => {
    const handler = (event: KeyboardEvent): void => {
      if (event.key === 'Escape') {
        event.stopPropagation();
        onClose();
        return;
      }
      if (event.key !== 'Tab' || !surfaceRef.current) return;

      const focusable = [...surfaceRef.current.querySelectorAll<HTMLElement>(FOCUSABLE)];
      if (focusable.length === 0) return;
      const first = focusable[0];
      const last = focusable[focusable.length - 1];
      if (!first || !last) return;

      if (event.shiftKey && document.activeElement === first) {
        event.preventDefault();
        last.focus();
      } else if (!event.shiftKey && document.activeElement === last) {
        event.preventDefault();
        first.focus();
      }
    };
    document.addEventListener('keydown', handler, true);
    return () => document.removeEventListener('keydown', handler, true);
  }, [onClose]);

  return createPortal(
    <div
      className={styles.modalLayer}
      onMouseDown={(event) => {
        if (event.target === event.currentTarget) onClose();
      }}
    >
      <div
        ref={surfaceRef}
        className={styles.modal}
        role="dialog"
        aria-modal="true"
        aria-label={title}
        tabIndex={-1}
      >
        <header className={styles.modalHeader}>
          <h2 className={styles.modalTitle}>{title}</h2>
          <Button variant="subtle" iconOnly onClick={onClose} aria-label="Close settings">
            ✕
          </Button>
        </header>
        <div className={styles.modalBody}>{children}</div>
        {footer ? <footer className={styles.modalHeader}>{footer}</footer> : null}
      </div>
    </div>,
    document.body,
  );
}
