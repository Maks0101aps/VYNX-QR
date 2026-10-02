const HEX_PATTERN = /^#?([0-9a-f]{3}|[0-9a-f]{6}|[0-9a-f]{8})$/i;

/**
 * Normalise a user typed colour into `#RRGGBB`, or `null` when invalid.
 *
 * Accepts the three and six digit forms with or without a leading `#`, and
 * always returns upper case so the value sent to Rust is stable.
 */
export function normaliseHex(value: string): string | null {
  const trimmed = value.trim();
  if (!HEX_PATTERN.test(trimmed)) return null;
  const hex = trimmed.replace('#', '');
  const expanded = hex.length === 3 ? [...hex].map((c) => c + c).join('') : hex;
  return `#${expanded.slice(0, 6).toUpperCase()}`;
}

/** The six digit, lower case form a native colour input understands. */
export function toPickerValue(value: string, fallback = '#000000'): string {
  const normalised = normaliseHex(value);
  if (normalised) return normalised.toLowerCase();
  const partial = value.replace('#', '');
  if (partial.length === 3 && /^[0-9a-f]{3}$/i.test(partial)) {
    return `#${[...partial]
      .map((c) => c + c)
      .join('')
      .toLowerCase()}`;
  }
  return fallback;
}
