import { describe, expect, it } from 'vitest';

import { normaliseHex, toPickerValue } from '@/lib/color';

describe('normaliseHex', () => {
  it('accepts the six digit form with and without a hash', () => {
    expect(normaliseHex('#1a1a1c')).toBe('#1A1A1C');
    expect(normaliseHex('1A1A1C')).toBe('#1A1A1C');
  });

  it('expands the three digit shorthand', () => {
    expect(normaliseHex('#abc')).toBe('#AABBCC');
    expect(normaliseHex('abc')).toBe('#AABBCC');
  });

  it('drops an alpha pair because QR colours are opaque', () => {
    expect(normaliseHex('#1A1A1CFF')).toBe('#1A1A1C');
  });

  it('trims surrounding whitespace', () => {
    expect(normaliseHex('  #FFFFFF  ')).toBe('#FFFFFF');
  });

  it('rejects anything that is not a HEX colour', () => {
    for (const value of ['', '#', '#12', '#12345', 'rebeccapurple', '#GGGGGG', 'rgb(0,0,0)']) {
      expect(normaliseHex(value), `expected \`${value}\` to be rejected`).toBeNull();
    }
  });
});

describe('toPickerValue', () => {
  it('passes valid colours through in lower case', () => {
    expect(toPickerValue('#1A1A1C')).toBe('#1a1a1c');
  });

  it('expands a shorthand so the native input keeps the same colour', () => {
    expect(toPickerValue('#abc')).toBe('#aabbcc');
  });

  it('falls back when the value cannot be understood', () => {
    expect(toPickerValue('nonsense')).toBe('#000000');
    expect(toPickerValue('nonsense', '#FFFFFF')).toBe('#FFFFFF');
  });
});
