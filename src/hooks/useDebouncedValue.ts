import { useEffect, useRef, useState, type MutableRefObject } from 'react';

/**
 * Value that only updates after it has been stable for `delay` milliseconds.
 * Used to keep QR generation off the keystroke path without adding a manual
 * "Generate" button.
 */
export function useDebouncedValue<T>(value: T, delay: number): T {
  const [debounced, setDebounced] = useState(value);

  useEffect(() => {
    const timer = window.setTimeout(() => setDebounced(value), delay);
    return () => window.clearTimeout(timer);
  }, [value, delay]);

  return debounced;
}

/** Latest value of `value`, without re-running effects. */
export function useLatest<T>(value: T): MutableRefObject<T> {
  const ref = useRef(value);
  ref.current = value;
  return ref;
}
