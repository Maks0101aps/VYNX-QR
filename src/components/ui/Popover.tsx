import {
  useCallback,
  useEffect,
  useLayoutEffect,
  useRef,
  useState,
  type ReactNode,
  type JSX,
} from 'react';
import { createPortal } from 'react-dom';

import styles from './ui.module.css';

interface PopoverProps {
  anchor: HTMLElement | null;
  onClose: () => void;
  children: ReactNode;
  /** Which edge of the anchor the popover lines up with. */
  align?: 'start' | 'end';
  labelledBy?: string;
}
const MARGIN = 6;
const GAP = 4;

/**
 * Fluent style popover anchored to a trigger element.
 *
 * Rendered in a portal so it is never clipped by the scrolling composer column.
 * Escape closes it and focus returns to the trigger.
 */
export function Popover({
  anchor,
  onClose,
  children,
  align = 'start',
  labelledBy,
}: PopoverProps): JSX.Element {
  const layerRef = useRef<HTMLDivElement>(null);
  const [position, setPosition] = useState<{ top: number; left: number } | null>(null);

  const update = useCallback(() => {
    if (!anchor) return;
    const rect = anchor.getBoundingClientRect();
    const width = layerRef.current?.offsetWidth ?? 240;
    const height = layerRef.current?.offsetHeight ?? 200;

    const left =
      align === 'end' ? Math.max(MARGIN, rect.right - width) : Math.max(MARGIN, rect.left);
    const below = rect.bottom + GAP;
    const fitsBelow = below + height <= window.innerHeight - MARGIN;
    const top = fitsBelow ? below : Math.max(MARGIN, rect.top - height - GAP);

    setPosition({ top, left: Math.min(left, window.innerWidth - width - MARGIN) });
  }, [anchor, align]);

  useLayoutEffect(() => {
    update();
  }, [update]);

  useEffect(() => {
    if (!anchor) return;
    const handler = (): void => update();
    window.addEventListener('resize', handler);
    window.addEventListener('scroll', handler, true);
    return () => {
      window.removeEventListener('resize', handler);
      window.removeEventListener('scroll', handler, true);
    };
  }, [anchor, update]);

  useEffect(() => {
    const handler = (event: KeyboardEvent): void => {
      if (event.key === 'Escape') {
        event.stopPropagation();
        onClose();
      }
    };
    document.addEventListener('keydown', handler, true);
    return () => document.removeEventListener('keydown', handler, true);
  }, [onClose]);

  useEffect(() => {
    anchor?.focus();
  }, [anchor]);

  return createPortal(
    <div
      ref={layerRef}
      className={styles.popoverLayer}
      onMouseDown={(event) => {
        if (event.target === event.currentTarget) onClose();
      }}
    >
      <div
        className={styles.popover}
        role="menu"
        aria-labelledby={labelledBy}
        style={{
          top: position?.top ?? -9999,
          left: position?.left ?? -9999,
          visibility: position ? 'visible' : 'hidden',
        }}
      >
        {children}
      </div>
    </div>,
    document.body,
  );
}

interface MenuItemProps {
  glyph: string;
  label: string;
  description?: string;
  onSelect: () => void;
  active: boolean;
  onActive: () => void;
}
export function MenuItem({
  glyph,
  label,
  description,
  onSelect,
  active,
  onActive,
}: MenuItemProps): JSX.Element {
  return (
    <button
      type="button"
      role="menuitem"
      className={styles.menuItem}
      data-active={active}
      onMouseEnter={onActive}
      onFocus={onActive}
      onClick={onSelect}
    >
      <span className={styles.menuItemGlyph} aria-hidden="true">
        {glyph}
      </span>
      <span className="grow" style={{ display: 'flex', flexDirection: 'column' }}>
        <span>{label}</span>
        {description ? (
          <span style={{ fontSize: 'var(--text-caption-size)', color: 'var(--text-tertiary)' }}>
            {description}
          </span>
        ) : null}
      </span>
    </button>
  );
}
