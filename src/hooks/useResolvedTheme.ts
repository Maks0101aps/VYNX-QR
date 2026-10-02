import { useEffect, useState } from 'react';

import type { ThemeMode } from '@/types/settings';

function prefersDark(): boolean {
  return typeof window.matchMedia === 'function'
    ? window.matchMedia('(prefers-color-scheme: dark)').matches
    : false;
}

/**
 * Resolve the effective theme, following the Windows setting when the user picks
 * "System". WebView2 keeps `prefers-color-scheme` in sync with Windows, so the
 * app follows light/dark switches live without a restart.
 */
export function useResolvedTheme(mode: ThemeMode): 'light' | 'dark' {
  const [systemDark, setSystemDark] = useState(prefersDark);

  useEffect(() => {
    if (mode !== 'system' || typeof window.matchMedia !== 'function') return;
    const query = window.matchMedia('(prefers-color-scheme: dark)');
    const listener = (event: MediaQueryListEvent): void => setSystemDark(event.matches);
    query.addEventListener('change', listener);
    return () => query.removeEventListener('change', listener);
  }, [mode]);

  if (mode === 'system') return systemDark ? 'dark' : 'light';
  return mode;
}
