/**
 * Shape of the error objects produced by `AppError` in `src-tauri/src/error.rs`.
 * Keep the codes in sync with `ErrorCode` on the Rust side.
 */
export interface AppError {
  code: string;
  message: string;
}
export function isAppError(value: unknown): value is AppError {
  return (
    typeof value === 'object' &&
    value !== null &&
    'code' in value &&
    'message' in value &&
    typeof (value as { message: unknown }).message === 'string'
  );
}

/**
 * Turn anything thrown across the IPC bridge into a sentence a user can act on.
 * Unexpected failures get a calm, generic message rather than a stack trace.
 */
export function toMessage(error: unknown): string {
  if (isAppError(error)) return error.message;
  if (error instanceof Error && error.message) return error.message;
  return 'VYNX QR ran into a problem. Please try again.';
}

export function toCode(error: unknown): string {
  return isAppError(error) ? error.code : 'unknown';
}
