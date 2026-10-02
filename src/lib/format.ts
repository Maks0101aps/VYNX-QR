import type { QrPayload } from '@/types/qr';

const WINDOWS_RESERVED = /[<>:"/\\|?*]/g;
const WINDOWS_TRAILING = /[. ]+$/;
/** Windows rejects C0 control characters in file names too. */
const CONTROL_CHARACTERS = /\p{Cc}/gu;

/**
 * Turn arbitrary content into a safe Windows file name.
 * Windows forbids `< > : " / \ | ? *` and control characters, and refuses names
 * that end with a dot or a space.
 */
export function sanitiseFileName(value: string, fallback = 'vynx-qr'): string {
  const cleaned = value
    .normalize('NFKC')
    .replace(CONTROL_CHARACTERS, '-')
    .replace(WINDOWS_RESERVED, '-')
    .replace(/\s+/g, '-')
    .replace(/-{2,}/g, '-')
    .replace(/^-+|-+$/g, '')
    .replace(WINDOWS_TRAILING, '')
    .slice(0, 64);
  return cleaned.length > 0 ? cleaned : fallback;
}

/** Prefix a slug, dropping the prefix when nothing usable is left. */
function withPrefix(prefix: string, value: string): string {
  return value.length > 0 ? `${prefix}-${value}` : prefix;
}

/** Human label for each payload kind, used when building file names. */
export function payloadSlug(payload: QrPayload): string {
  switch (payload.type) {
    case 'url': {
      const host = sanitiseFileName(payload.url.replace(/^https?:\/\//i, ''), '');
      return host.length > 0 ? host : 'link';
    }
    case 'wifi':
      return withPrefix('wifi', sanitiseFileName(payload.ssid, ''));
    case 'vCard':
      return withPrefix(
        'contact',
        sanitiseFileName(`${payload.firstName} ${payload.lastName}`, ''),
      );
    case 'email':
      return withPrefix('email', sanitiseFileName(payload.to, ''));
    case 'phone':
      return withPrefix('phone', sanitiseFileName(payload.number, ''));
    case 'sms':
      return withPrefix('sms', sanitiseFileName(payload.number, ''));
    case 'geo':
      return withPrefix('location', sanitiseFileName(payload.label, ''));
    case 'text':
    default: {
      const firstLine = payload.text.split('\n')[0] ?? '';
      return withPrefix('text', sanitiseFileName(firstLine, ''));
    }
  }
}

/**
 * Suggested file name for a payload.
 *
 * `github.com` becomes `github.com-qr.png`, a Wi-Fi network called `VYNX Home`
 * becomes `wifi-VYNX-Home-qr.png`, and content with nothing usable in it falls
 * back to `text-qr.png` or `link-qr.png`.
 */
export function suggestFileName(payload: QrPayload, format: 'png' | 'svg'): string {
  return `${sanitiseFileName(`${payloadSlug(payload)}-qr`)}.${format}`;
}
