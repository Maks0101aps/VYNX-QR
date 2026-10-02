import { useCallback, useEffect, useMemo, useRef, useState, type JSX } from 'react';
import { open, save } from '@tauri-apps/plugin-dialog';

import { ActionBar } from '@/components/ActionBar/ActionBar';
import { CustomizePanel } from '@/components/CustomizePanel/CustomizePanel';
import { DetectionChip } from '@/components/DetectionChip/DetectionChip';
import { PayloadForms } from '@/components/PayloadForms/PayloadForms';
import { QRPreview } from '@/components/QRPreview/QRPreview';
import { SettingsModal } from '@/components/SettingsModal/SettingsModal';
import { SmartInput } from '@/components/SmartInput/SmartInput';
import { StatusBar } from '@/components/StatusBar/StatusBar';
import { TitleBar } from '@/components/TitleBar/TitleBar';
import { Toasts } from '@/components/Toasts/Toasts';
import { Segmented } from '@/components/ui/Segmented';
import { useDebouncedValue } from '@/hooks/useDebouncedValue';
import { useGlobalShortcuts } from '@/hooks/useGlobalShortcuts';
import { useResolvedTheme } from '@/hooks/useResolvedTheme';
import { useToasts } from '@/hooks/useToasts';
import { DEFAULT_STYLE, commands, type ExportPayload } from '@/lib/commands';
import { fileToLogoInput } from '@/lib/file';
import { suggestFileName } from '@/lib/format';
import type {
  Analysis,
  ExportFormat,
  LogoInput,
  QrPayload,
  QrStyle,
  RenderResult,
  SystemInfo,
} from '@/types/qr';
import { toMessage } from '@/types/errors';
import { DEFAULT_SETTINGS, type Settings } from '@/types/settings';
import styles from '@/App.module.css';

const ANALYSIS_DELAY = 160;

/**
 * The whole application.
 *
 * Everything that decides what a QR code contains lives in Rust: the front end
 * only holds the input text, the chosen style and the last render result. The
 * flow is deliberately automatic — there is no Generate button, because the
 * detector classifies the text and the renderer produces the code.
 */
export function App(): JSX.Element {
  const [input, setInput] = useState('');
  const [analysis, setAnalysis] = useState<Analysis | null>(null);
  const [usingOriginalText, setUsingOriginalText] = useState(false);
  const [style, setStyle] = useState<QrStyle>(DEFAULT_STYLE);
  const [result, setResult] = useState<RenderResult | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [copying, setCopying] = useState(false);
  const [saving, setSaving] = useState(false);
  const [customizeOpen, setCustomizeOpen] = useState(false);
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [settings, setSettings] = useState<Settings>(DEFAULT_SETTINGS);
  const [systemInfo, setSystemInfo] = useState<SystemInfo | null>(null);
  const [mode, setMode] = useState<'auto' | 'structured'>('auto');
  const [structured, setStructured] = useState<QrPayload>({
    type: 'wifi',
    ssid: '',
    password: '',
    security: 'wpa',
    hidden: false,
  });

  const { toasts, push, dismiss } = useToasts();
  const theme = useResolvedTheme(settings.theme);
  const debouncedInput = useDebouncedValue(input, ANALYSIS_DELAY);

  /* ------------------------------------------------------------------ theme */

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
  }, [theme]);

  useEffect(() => {
    const accent =
      settings.useWindowsAccent && systemInfo?.accentColor ? systemInfo.accentColor : null;
    const root = document.documentElement.style;
    if (accent) {
      root.setProperty('--accent', accent);
      root.removeProperty('--accent-hover');
      root.removeProperty('--accent-pressed');
    } else {
      root.removeProperty('--accent');
      root.removeProperty('--accent-hover');
      root.removeProperty('--accent-pressed');
    }
  }, [settings.useWindowsAccent, systemInfo]);

  /* ------------------------------------------------------- start-up hydrate */

  useEffect(() => {
    let cancelled = false;

    void (async () => {
      try {
        const [loadedSettings, info] = await Promise.all([
          commands.loadSettings(),
          commands.systemInfo(),
        ]);
        if (cancelled) return;
        setSettings(loadedSettings);
        setSystemInfo(info);
        setStyle((current) => ({
          ...current,
          moduleStyle: loadedSettings.defaultModuleStyle,
        }));

        if (loadedSettings.clipboardCheck) {
          const text = await commands.readClipboardText();
          if (cancelled || !text || text.trim().length === 0) return;
          setInput(text.trim());
          if (loadedSettings.autoPaste) {
            push('Encoded the clipboard contents', 'neutral');
          } else {
            push('Clipboard content added — edit it if you like', 'neutral');
          }
        }
      } catch {
        // A missing settings file or a locked clipboard must never stop start-up.
        if (!cancelled) push('Some preferences could not be loaded', 'danger');
      }
    })();

    return () => {
      cancelled = true;
    };
  }, [push]);

  /* -------------------------------------------------------------- detection */

  /** Non-null when the user picked a structured type instead of free-form input. */
  const specialPayload = useMemo<QrPayload | null>(() => {
    if (mode === 'auto') return null;
    if (mode === 'structured') return structured;
    return null;
  }, [mode, structured]);

  useEffect(() => {
    let cancelled = false;
    const text = debouncedInput.trim();

    if (text.length === 0) {
      setAnalysis(null);
      setResult(null);
      setError(null);
      return;
    }

    void (async () => {
      try {
        const found = await commands.analyzeInput(text);
        if (!cancelled) {
          setAnalysis(found);
          setUsingOriginalText(false);
        }
      } catch (failure) {
        if (!cancelled) {
          setAnalysis(null);
          setError(toMessage(failure));
        }
      }
    })();

    return () => {
      cancelled = true;
    };
  }, [debouncedInput]);

  /* ---------------------------------------------------------------- render */

  const payload = useMemo<QrPayload | null>(() => {
    if (specialPayload) return specialPayload;
    if (!analysis) return null;
    if (usingOriginalText) return { type: 'text', text: input.trim() };
    return analysis.payload;
  }, [specialPayload, analysis, usingOriginalText, input]);

  // Latest values for callbacks that must not re-create on every keystroke.
  const stateRef = useRef({ payload, style, settings });
  stateRef.current = { payload, style, settings };

  useEffect(() => {
    if (!payload) return;
    let cancelled = false;
    setBusy(true);

    const request = {
      payload,
      style,
      ecLevel: settings.defaultErrorCorrection,
      sizePx: 1024,
    };

    void (async () => {
      try {
        const rendered = await commands.renderQr(request);
        if (!cancelled) {
          setResult(rendered);
          setError(null);
        }
      } catch (failure) {
        if (!cancelled) {
          setResult(null);
          setError(toMessage(failure));
        }
      } finally {
        if (!cancelled) setBusy(false);
      }
    })();

    return () => {
      cancelled = true;
    };
  }, [payload, style, settings.defaultErrorCorrection]);

  /* ---------------------------------------------------------------- export */

  const exportPayload = useCallback((): ExportPayload | null => {
    const current = stateRef.current;
    if (!current.payload) return null;
    return {
      payload: current.payload,
      style: current.style,
      ecLevel: current.settings.defaultErrorCorrection,
    };
  }, []);

  const handleCopy = useCallback(async (): Promise<void> => {
    const payload = exportPayload();
    if (!payload) return;
    setCopying(true);
    try {
      await commands.copyQr(payload);
      push('QR code copied to the clipboard', 'success');
    } catch (failure) {
      push(toMessage(failure), 'danger');
    } finally {
      setCopying(false);
    }
  }, [exportPayload, push]);

  const handleSave = useCallback(
    async (format: ExportFormat): Promise<void> => {
      const payload = exportPayload();
      if (!payload) return;
      setSaving(true);
      try {
        const suggested = suggestFileName(payload.payload, format);
        const chosen = await save({
          defaultPath: suggested,
          filters: [
            {
              name: format === 'svg' ? 'SVG vector' : 'PNG image',
              extensions: [format],
            },
          ],
        });
        if (!chosen) return;
        await commands.exportQr(payload, chosen, format, stateRef.current.settings.defaultSize);
        push('QR code saved', 'success');
      } catch (failure) {
        push(toMessage(failure), 'danger');
      } finally {
        setSaving(false);
      }
    },
    [exportPayload, push],
  );

  const handlePickLogo = useCallback(async (): Promise<void> => {
    try {
      const chosen = await open({
        multiple: false,
        directory: false,
        filters: [{ name: 'Images', extensions: ['png', 'jpg', 'jpeg', 'webp'] }],
      });
      if (typeof chosen !== 'string') return;
      const asset = await commands.loadLogo(chosen);
      setStyle((current) => ({ ...current, logo: { name: asset.name, data: asset.data } }));
      push('Logo added', 'success');
    } catch (failure) {
      push(toMessage(failure), 'danger');
    }
  }, [push]);

  const handleDropLogo = useCallback(
    (file: File): void => {
      void (async () => {
        try {
          const logo: LogoInput = await fileToLogoInput(file);
          setStyle((current) => ({ ...current, logo }));
          push('Logo added', 'success');
        } catch (failure) {
          push(toMessage(failure), 'danger');
        }
      })();
    },
    [push],
  );

  /* -------------------------------------------------------------- settings */

  const handleSettingsChange = useCallback(
    (next: Settings): void => {
      setSettings(next);
      setStyle((current) => ({ ...current, moduleStyle: next.defaultModuleStyle }));
      void commands.saveSettings(next).catch(() => {
        push('Preferences could not be saved', 'danger');
      });
    },
    [push],
  );

  /* ------------------------------------------------------------- shortcuts */

  useGlobalShortcuts(
    {
      c: () => void handleCopy(),
      s: () => void handleSave(stateRef.current.settings.defaultFormat),
      e: () => setCustomizeOpen((open_) => !open_),
      ',': () => setSettingsOpen(true),
    },
    Boolean(exportPayload()),
  );

  /* ----------------------------------------------------------------- render */

  const canExport = Boolean(payload);

  return (
    <div className={styles.app}>
      <TitleBar onOpenSettings={() => setSettingsOpen(true)} />

      <div className={styles.body}>
        <div className={styles.composer}>
          <div className={styles.composerSection}>
            <div className={styles.field}>
              <span className={styles.fieldLabel}>Mode</span>
              <Segmented
                label="Input mode"
                value={mode}
                onChange={(next) => setMode(next)}
                options={[
                  { value: 'auto', label: 'Smart', title: 'Detect the type automatically' },
                  { value: 'structured', label: 'Custom', title: 'Fill in a specific form' },
                ]}
                fullWidth
              />
            </div>

            {mode === 'auto' ? (
              <>
                <SmartInput value={input} onChange={setInput} onClear={() => setInput('')} />
                <DetectionChip
                  analysis={analysis}
                  usingOriginalText={usingOriginalText}
                  onToggleOriginal={() => setUsingOriginalText((current) => !current)}
                />
              </>
            ) : (
              <PayloadForms payload={structured} onChange={setStructured} />
            )}
          </div>

          {customizeOpen ? (
            <CustomizePanel
              style={style}
              onChange={setStyle}
              onPickLogo={() => void handlePickLogo()}
              onClearLogo={() => setStyle((current) => ({ ...current, logo: null }))}
              onClose={() => setCustomizeOpen(false)}
              accent={systemInfo?.accentColor ?? null}
              systemInfo={systemInfo}
              settings={settings}
            />
          ) : null}

          <div className={styles.composerFooter}>
            <ActionBar
              disabled={!canExport}
              copying={copying}
              saving={saving}
              onCopy={() => void handleCopy()}
              onSave={(format) => void handleSave(format)}
              onCustomize={() => setCustomizeOpen((open_) => !open_)}
              customizeOpen={customizeOpen}
            />
          </div>
        </div>

        <div className={styles.previewPane}>
          <QRPreview
            result={result}
            busy={busy}
            hasLogoSlot={customizeOpen}
            onDropLogo={handleDropLogo}
          />
          <div className={styles.previewFooter}>
            <StatusBar result={result} error={error} size={settings.defaultSize} />
          </div>
        </div>
      </div>

      <Toasts toasts={toasts} onDismiss={dismiss} />

      {settingsOpen ? (
        <SettingsModal
          settings={settings}
          systemInfo={systemInfo}
          onChange={handleSettingsChange}
          onClose={() => setSettingsOpen(false)}
        />
      ) : null}
    </div>
  );
}
