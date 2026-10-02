import { useEffect, useRef } from 'react';

/**
 * Global keyboard shortcuts for the window.
 *
 * These are plain in-app shortcuts: nothing is registered with the OS, so
 * VYNX QR never grabs a key combination system wide.
 */
export function useGlobalShortcuts(handlers: Record<string, () => void>, enabled = true): void {
  const ref = useRef(handlers);
  ref.current = handlers;

  useEffect(() => {
    if (!enabled) return;

    const onKeyDown = (event: KeyboardEvent): void => {
      if (!event.ctrlKey || event.altKey || event.metaKey) return;
      const key = event.key.toLowerCase();
      const handler = ref.current[key];
      if (!handler) return;
      event.preventDefault();
      handler();
    };

    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [enabled]);
}
