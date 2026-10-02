import { beforeEach, describe, expect, it, vi } from 'vitest';
import { render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';

import type { Analysis, RenderResult, SystemInfo } from '@/types/qr';
import { DEFAULT_SETTINGS } from '@/types/settings';

type InvokeArgs = Record<string, unknown>;
type Invoke = (command: string, args?: InvokeArgs) => Promise<unknown>;

const invoke = vi.fn<Invoke>();
vi.mock('@tauri-apps/api/core', () => ({
  invoke: (command: string, args?: InvokeArgs) => invoke(command, args),
}));
vi.mock('@tauri-apps/plugin-dialog', () => ({ open: vi.fn(), save: vi.fn() }));

import { App } from './App';

const SYSTEM_INFO: SystemInfo = {
  osBuild: '10.0.22631',
  isWindows11: true,
  accentColor: '#0078D4',
  development: false,
};

const RENDER_RESULT: RenderResult = {
  pngBase64: 'AAAA',
  width: 1024,
  height: 1024,
  modules: 25,
  totalModules: 33,
  quietZone: 4,
  version: 2,
  ecLevel: 'h',
  ecAdjusted: false,
  contrast: 21,
  verification: { status: 'verified', decoded: null, reduced: null },
  warnings: [],
  encoded: 'https://github.com',
};

function analysisFor(input: string): Analysis {
  return {
    input,
    kind: 'url',
    payload: { type: 'url', url: input },
    encoded: input,
    normalization: 'https:// will be added',
    notice: null,
  };
}

/** Answers each command the app issues on start-up and while rendering. */
function stubCore(): void {
  invoke.mockImplementation((command, args) => {
    switch (command) {
      case 'load_settings':
        return Promise.resolve(DEFAULT_SETTINGS);
      case 'system_info':
        return Promise.resolve(SYSTEM_INFO);
      case 'read_clipboard_text':
        return Promise.resolve(null);
      case 'analyze_input': {
        const typed = (args as { input?: unknown } | undefined)?.input;
        return Promise.resolve(analysisFor(typeof typed === 'string' ? typed : ''));
      }
      case 'render_qr':
        return Promise.resolve(RENDER_RESULT);
      case 'save_settings':
        return Promise.resolve(null);
      default:
        return Promise.reject(new Error(`unexpected command: ${command}`));
    }
  });
}

/** The payload of the most recent `render_qr` call. */
function lastRenderedPayload(): Record<string, unknown> | null {
  const calls = invoke.mock.calls.filter(([command]) => command === 'render_qr');
  const last = calls[calls.length - 1];
  const args = last?.[1] as { request?: { payload: Record<string, unknown> } } | undefined;
  return args?.request?.payload ?? null;
}

describe('App', () => {
  beforeEach(() => {
    invoke.mockReset();
    stubCore();
    document.documentElement.removeAttribute('style');
  });

  it('shows an empty state before anything is typed', async () => {
    render(<App />);
    expect(await screen.findByText('Create a QR code')).toBeInTheDocument();
    expect(screen.queryByRole('img', { name: 'Generated QR code' })).toBeNull();
  });

  it('detects content and renders a verified code automatically', async () => {
    const user = userEvent.setup();
    render(<App />);

    await user.type(screen.getByRole('textbox', { name: 'Create QR' }), 'github.com');

    const preview = await screen.findByRole('img', { name: 'Generated QR code' });
    expect(preview).toHaveAttribute('src', 'data:image/png;base64,AAAA');
    expect(await screen.findByText('Scan verified')).toBeInTheDocument();
    expect(screen.getByText('URL')).toBeInTheDocument();
  });

  it('tells the user when https:// will be added, and lets them keep the text', async () => {
    const user = userEvent.setup();
    render(<App />);

    await user.type(screen.getByRole('textbox', { name: 'Create QR' }), 'github.com');

    expect(await screen.findByText('https:// will be added')).toBeInTheDocument();
    await user.click(screen.getByRole('button', { name: 'Use original text' }));

    await waitFor(() => expect(lastRenderedPayload()?.type).toBe('text'));
  });

  it('applies the Windows accent colour to the document', async () => {
    render(<App />);
    await waitFor(() => {
      expect(document.documentElement.style.getPropertyValue('--accent')).toBe('#0078D4');
    });
  });

  it('reports an error from the core instead of showing a stale code', async () => {
    invoke.mockImplementation((command) => {
      switch (command) {
        case 'load_settings':
          return Promise.resolve(DEFAULT_SETTINGS);
        case 'system_info':
          return Promise.resolve(SYSTEM_INFO);
        case 'read_clipboard_text':
          return Promise.resolve(null);
        case 'analyze_input':
          return Promise.resolve(analysisFor('github.com'));
        case 'render_qr':
          return Promise.reject(
            Object.assign(new Error('That content does not fit in a QR code.'), {
              code: 'ContentTooLong',
            }),
          );
        default:
          return Promise.resolve(null);
      }
    });

    const user = userEvent.setup();
    render(<App />);
    await user.type(screen.getByRole('textbox', { name: 'Create QR' }), 'github.com');

    expect(await screen.findByText('That content does not fit in a QR code.')).toBeInTheDocument();
    expect(screen.queryByRole('img', { name: 'Generated QR code' })).toBeNull();
  });

  it('copies the rendered bitmap to the clipboard', async () => {
    const user = userEvent.setup();
    render(<App />);
    await user.type(screen.getByRole('textbox', { name: 'Create QR' }), 'github.com');
    await screen.findByRole('img', { name: 'Generated QR code' });

    invoke.mockImplementation((command) =>
      command === 'copy_qr' ? Promise.resolve(null) : Promise.resolve(null),
    );

    await user.click(screen.getByRole('button', { name: 'Copy QR code image to the clipboard' }));
    expect(await screen.findByText('QR code copied to the clipboard')).toBeInTheDocument();
  });

  it('opens the customize panel on request', async () => {
    const user = userEvent.setup();
    render(<App />);

    await user.click(screen.getByRole('button', { name: 'Customize the QR code' }));
    expect(await screen.findByRole('region', { name: 'Customize' })).toBeInTheDocument();
    expect(screen.getByRole('radiogroup', { name: 'Module shape' })).toBeInTheDocument();
  });

  it('opens settings from the title bar', async () => {
    const user = userEvent.setup();
    render(<App />);

    await user.click(screen.getByRole('button', { name: 'Settings' }));
    expect(await screen.findByRole('dialog', { name: 'Settings' })).toBeInTheDocument();
  });

  it('switches to a structured form and encodes what is typed there', async () => {
    const user = userEvent.setup();
    render(<App />);

    await user.click(screen.getByRole('radio', { name: 'Custom' }));
    await user.type(await screen.findByLabelText('Network name (SSID)'), 'VYNX Home');

    await waitFor(() => {
      expect(lastRenderedPayload()).toMatchObject({ type: 'wifi', ssid: 'VYNX Home' });
    });
  });

  it('changes the structured payload type', async () => {
    const user = userEvent.setup();
    render(<App />);

    await user.click(screen.getByRole('radio', { name: 'Custom' }));
    await user.click(await screen.findByRole('radio', { name: 'Message' }));

    expect(await screen.findByLabelText('Number')).toBeInTheDocument();
    expect(screen.getByLabelText('Message', { selector: 'textarea' })).toBeInTheDocument();
  });

  it('hides the password field for an open network', async () => {
    const user = userEvent.setup();
    render(<App />);

    await user.click(screen.getByRole('radio', { name: 'Custom' }));
    expect(await screen.findByLabelText('Password')).toBeInTheDocument();

    await user.selectOptions(screen.getByLabelText('Security'), 'none');
    expect(screen.queryByLabelText('Password')).toBeNull();
  });
});
