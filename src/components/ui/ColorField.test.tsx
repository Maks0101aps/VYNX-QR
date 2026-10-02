import { describe, expect, it, vi } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';

import { ColorField } from './ColorField';
import { toPickerValue } from '@/lib/color';

describe('ColorField', () => {
  it('labels the swatch and the HEX field', () => {
    render(
      <ColorField label="Foreground" value="#1A1A1C" pickerValue="#1a1a1c" onChange={vi.fn()} />,
    );
    expect(screen.getByRole('textbox', { name: 'Foreground HEX value' })).toHaveValue('#1A1A1C');
  });

  it('reports invalid HEX without discarding the edit', async () => {
    const user = userEvent.setup();
    const onChange = vi.fn();
    render(<ColorField label="Foreground" value="#zz" pickerValue="#000000" onChange={onChange} />);

    const field = screen.getByRole('textbox', { name: 'Foreground HEX value' });
    expect(field).toHaveAttribute('aria-invalid', 'true');
    expect(screen.getByText(/Use a HEX colour/)).toBeInTheDocument();

    await user.type(field, '1');
    expect(onChange).toHaveBeenLastCalledWith('#zz1');
  });

  it('normalises a valid value on blur', async () => {
    const user = userEvent.setup();
    const onChange = vi.fn();
    render(
      <ColorField label="Background" value="#abc" pickerValue="#aabbcc" onChange={onChange} />,
    );

    await user.click(screen.getByRole('textbox', { name: 'Background HEX value' }));
    await user.tab();
    expect(onChange).toHaveBeenLastCalledWith('#AABBCC');
  });

  it('marks a valid field as not invalid', () => {
    render(
      <ColorField label="Foreground" value="#FFFFFF" pickerValue="#ffffff" onChange={vi.fn()} />,
    );
    expect(screen.getByRole('textbox', { name: 'Foreground HEX value' })).toHaveAttribute(
      'aria-invalid',
      'false',
    );
  });

  it('gives the native picker a usable value', () => {
    expect(toPickerValue('#abc')).toBe('#aabbcc');
  });
});
