import { describe, expect, it } from 'vitest';

import { payloadSlug, sanitiseFileName, suggestFileName } from '@/lib/format';
import type { QrPayload } from '@/types/qr';

describe('sanitiseFileName', () => {
  it('replaces characters Windows forbids', () => {
    expect(sanitiseFileName('a<b>c:d"e/f\\g|h?i*j')).toBe('a-b-c-d-e-f-g-h-i-j');
  });

  it('replaces control characters', () => {
    expect(sanitiseFileName('before\u0007after')).toBe('before-after');
    expect(sanitiseFileName('a\u0000b\u001Fc')).toBe('a-b-c');
  });

  it('never ends with a dot or a space', () => {
    expect(sanitiseFileName('report.')).toBe('report');
    expect(sanitiseFileName('report ')).toBe('report');
  });

  it('collapses runs of separators', () => {
    expect(sanitiseFileName('a   b')).toBe('a-b');
  });

  it('keeps non Latin scripts', () => {
    expect(sanitiseFileName('Привіт')).toBe('Привіт');
  });

  it('uses the fallback when nothing usable is left', () => {
    expect(sanitiseFileName('///')).toBe('vynx-qr');
    expect(sanitiseFileName('', 'contact')).toBe('contact');
  });
});

describe('payloadSlug', () => {
  it('drops the scheme from a URL', () => {
    const payload: QrPayload = { type: 'url', url: 'https://github.com/VYNX' };
    expect(payloadSlug(payload)).toBe('github.com-VYNX');
  });

  it('names a Wi-Fi network after the SSID', () => {
    const payload: QrPayload = {
      type: 'wifi',
      ssid: 'VYNX Home',
      password: 'secret',
      security: 'wpa',
      hidden: false,
    };
    expect(payloadSlug(payload)).toBe('wifi-VYNX-Home');
  });

  it('uses a stable fallback for an unnamed network', () => {
    const payload: QrPayload = {
      type: 'wifi',
      ssid: '',
      password: 'secret',
      security: 'none',
      hidden: true,
    };
    expect(payloadSlug(payload)).toBe('wifi');
  });

  it('uses only the first line of a text payload', () => {
    const payload: QrPayload = { type: 'text', text: 'First line\nSecond line' };
    expect(payloadSlug(payload)).toBe('text-First-line');
  });
});

describe('suggestFileName', () => {
  it('matches the documented example', () => {
    const payload: QrPayload = { type: 'url', url: 'https://github.com' };
    expect(suggestFileName(payload, 'png')).toBe('github.com-qr.png');
  });

  it('uses the requested extension', () => {
    const payload: QrPayload = { type: 'url', url: 'https://github.com' };
    expect(suggestFileName(payload, 'svg')).toBe('github.com-qr.svg');
  });

  it('falls back for content with no usable characters', () => {
    const payload: QrPayload = { type: 'text', text: '///' };
    expect(suggestFileName(payload, 'png')).toBe('text-qr.png');
  });

  it('falls back for a URL with no usable host', () => {
    const payload: QrPayload = { type: 'url', url: 'https://///' };
    expect(suggestFileName(payload, 'svg')).toBe('link-qr.svg');
  });
});
