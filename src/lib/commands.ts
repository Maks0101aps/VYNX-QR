import { invoke } from '@tauri-apps/api/core';

import type { AppError } from '@/types/errors';
import type {
  Analysis,
  ExportFormat,
  LogoAssetPayload,
  QrStyle,
  RenderRequest,
  RenderResult,
  SystemInfo,
} from '@/types/qr';
import type { Settings } from '@/types/settings';

export type ExportPayload = Omit<RenderRequest, 'sizePx'>;

/**
 * Invoke a Tauri command and normalise failures into `AppError`, so callers can
 * always show a sentence written for humans.
 */
async function call<T>(command: string, args?: Record<string, unknown>): Promise<T> {
  try {
    return await invoke<T>(command, args);
  } catch (error) {
    throw asFailure(error);
  }
}

/**
 * Turn anything the IPC bridge rejects with into a real `Error`.
 *
 * The Rust side rejects with `{ code, message }`, and keeping `code` on the
 * thrown object means `isAppError` still recognises it while the value is a
 * legitimate `Error` for every other consumer.
 */
function asFailure(error: unknown): Error & AppError {
  const normalised = normalise(error);
  return Object.assign(new Error(normalised.message), { code: normalised.code });
}

function normalise(error: unknown): AppError {
  if (typeof error === 'object' && error !== null && 'message' in error) {
    const candidate = error as AppError;
    if (typeof candidate.message === 'string') {
      return { code: candidate.code ?? 'unknown', message: candidate.message };
    }
  }
  if (typeof error === 'string' && error.length > 0) {
    return { code: 'unknown', message: error };
  }
  return { code: 'unknown', message: 'VYNX QR ran into a problem. Please try again.' };
}

export const commands = {
  renderQr: (request: RenderRequest): Promise<RenderResult> =>
    call<RenderResult>('render_qr', { request }),

  copyQr: (request: ExportPayload): Promise<void> =>
    call<void>('copy_qr', { request: { ...request, sizePx: 1024 } }),

  exportQr: (
    request: ExportPayload,
    path: string,
    format: ExportFormat,
    sizePx: number,
  ): Promise<void> =>
    call<void>('export_qr', {
      request: { ...request, sizePx },
      path,
      format,
    }),

  readClipboardText: (): Promise<string | null> => call<string | null>('read_clipboard_text'),

  analyzeInput: (input: string): Promise<Analysis | null> =>
    call<Analysis | null>('analyze_input', { input }),

  loadLogo: (path: string): Promise<LogoAssetPayload> =>
    call<LogoAssetPayload>('load_logo', { path }),

  loadSettings: (): Promise<Settings> => call<Settings>('load_settings'),

  saveSettings: (settings: Settings): Promise<void> => call<void>('save_settings', { settings }),

  systemInfo: (): Promise<SystemInfo> => call<SystemInfo>('system_info'),
};

/** Default styling, kept in step with `QrStyle::default()` on the Rust side. */
export const DEFAULT_STYLE: QrStyle = {
  moduleStyle: 'square',
  foreground: '#000000',
  background: '#FFFFFF',
  quietZone: 4,
  logoRatio: 0.2,
  logo: null,
};
