import { defineConfig } from 'vitest/config';
import react from '@vitejs/plugin-react';
import { fileURLToPath, URL } from 'node:url';

const host = process.env['TAURI_DEV_HOST'];

// https://vite.dev/config/
export default defineConfig(() => ({
  plugins: [react()],

  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },

  // Tauri expects a fixed port and must not fall back to another one.
  clearScreen: false,
  server: {
    port: 1420,
    strictPort: true,
    host: host ?? false,
    // Only a device on the network needs the separate HMR socket.
    ...(host ? { hmr: { protocol: 'ws' as const, host, port: 1421 } } : {}),
    watch: {
      ignored: ['**/src-tauri/**'],
    },
  },

  build: {
    // Windows 11 ships WebView2, which is evergreen Chromium.
    target: 'chrome120',
    outDir: 'dist',
    emptyOutDir: true,
    sourcemap: false,
    cssCodeSplit: true,
    reportCompressedSize: false,
    rollupOptions: {
      output: {
        entryFileNames: 'assets/[name]-[hash].js',
        chunkFileNames: 'assets/[name]-[hash].js',
        assetFileNames: 'assets/[name]-[hash][extname]',
      },
    },
  },

  test: {
    globals: true,
    environment: 'jsdom',
    setupFiles: ['./src/test/setup.ts'],
    include: ['src/**/*.{test,spec}.{ts,tsx}'],
    css: false,
  },
}));
