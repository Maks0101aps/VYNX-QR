import '@testing-library/jest-dom/vitest';

/**
 * jsdom does not implement the pieces of the platform the app touches, so the
 * minimum is stubbed here rather than in every test.
 */
if (typeof window.matchMedia !== 'function') {
  Object.defineProperty(window, 'matchMedia', {
    writable: true,
    value: (query: string): MediaQueryList => ({
      matches: false,
      media: query,
      onchange: null,
      addEventListener: () => undefined,
      removeEventListener: () => undefined,
      addListener: () => undefined,
      removeListener: () => undefined,
      dispatchEvent: () => false,
    }),
  });
}

if (typeof URL.createObjectURL !== 'function') {
  URL.createObjectURL = () => 'blob:vynx-qr';
  URL.revokeObjectURL = () => undefined;
}
