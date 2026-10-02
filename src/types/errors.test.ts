import { describe, expect, it } from 'vitest';

import { isAppError, toCode, toMessage } from '@/types/errors';

describe('isAppError', () => {
  it('recognises the shape the Rust core rejects with', () => {
    expect(isAppError({ code: 'ContentTooLong', message: 'Too long' })).toBe(true);
  });

  it('rejects anything else', () => {
    expect(isAppError(null)).toBe(false);
    expect(isAppError('boom')).toBe(false);
    expect(isAppError({ code: 'x' })).toBe(false);
    expect(isAppError({ message: 42 })).toBe(false);
  });
});

describe('toMessage', () => {
  it('passes an AppError message through', () => {
    expect(toMessage({ code: 'EmptyInput', message: 'Nothing to encode.' })).toBe(
      'Nothing to encode.',
    );
  });

  it('uses a real Error message', () => {
    expect(toMessage(new Error('boom'))).toBe('boom');
  });

  it('falls back to a calm sentence for anything unexpected', () => {
    const fallback = 'VYNX QR ran into a problem. Please try again.';
    expect(toMessage(undefined)).toBe(fallback);
    expect(toMessage(42)).toBe(fallback);
  });
});

describe('toCode', () => {
  it('returns the code when there is one', () => {
    expect(toCode({ code: 'InvalidUrl', message: 'x' })).toBe('InvalidUrl');
  });

  it('reports `unknown` for anything else', () => {
    expect(toCode(new Error('boom'))).toBe('unknown');
  });
});
