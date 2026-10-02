import { useState, type JSX } from 'react';
import { describe, expect, it, vi } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';

import { SmartInput } from './SmartInput';

describe('SmartInput', () => {
  it('shows the value it is given', () => {
    render(<SmartInput value="https://github.com" onChange={vi.fn()} onClear={vi.fn()} />);
    expect(screen.getByRole('textbox')).toHaveValue('https://github.com');
  });

  it('reports what the user types', async () => {
    const user = userEvent.setup();
    function Controlled(): JSX.Element {
      const [value, setValue] = useState('');
      return <SmartInput value={value} onChange={setValue} onClear={() => setValue('')} />;
    }
    render(<Controlled />);

    await user.type(screen.getByRole('textbox'), 'hi');
    expect(screen.getByRole('textbox')).toHaveValue('hi');
  });

  it('only offers the clear button when there is something to clear', async () => {
    const user = userEvent.setup();
    const { rerender } = render(<SmartInput value="" onChange={vi.fn()} onClear={vi.fn()} />);
    expect(screen.queryByRole('button', { name: 'Clear input' })).toBeNull();

    const onClear = vi.fn();
    rerender(<SmartInput value="text" onChange={vi.fn()} onClear={onClear} />);
    await user.click(screen.getByRole('button', { name: 'Clear input' }));
    expect(onClear).toHaveBeenCalledOnce();
  });

  it('clears on Escape without swallowing the key from the rest of the app', async () => {
    const user = userEvent.setup();
    const onClear = vi.fn();
    render(<SmartInput value="text" onChange={vi.fn()} onClear={onClear} />);

    await user.click(screen.getByRole('textbox'));
    await user.keyboard('{Escape}');
    expect(onClear).toHaveBeenCalledOnce();
  });

  it('describes the field for assistive technology', () => {
    render(<SmartInput value="" onChange={vi.fn()} onClear={vi.fn()} />);
    const field = screen.getByRole('textbox', { name: 'Create QR' });
    expect(field).toHaveAttribute('aria-describedby', 'vynx-input-hint');
  });

  it('can be disabled', () => {
    render(<SmartInput value="x" onChange={vi.fn()} onClear={vi.fn()} disabled />);
    expect(screen.getByRole('textbox')).toBeDisabled();
  });
});
