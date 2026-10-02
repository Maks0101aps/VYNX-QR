import type { JSX } from 'react';

import type { Settings, ThemeMode } from '@/types/settings';
import type { EcLevel, ModuleStyle, SystemInfo } from '@/types/qr';
import { Button } from '@/components/ui/Button';
import { Field } from '@/components/ui/Field';
import { Modal } from '@/components/ui/Modal';
import { Segmented } from '@/components/ui/Segmented';
import { InfoNotice, Toggle } from '@/components/ui/Toggle';
import ui from '@/components/ui/ui.module.css';

const SIZES = [256, 512, 1024, 2048] as const;

interface SettingsModalProps {
  settings: Settings;
  systemInfo: SystemInfo | null;
  onChange: (settings: Settings) => void;
  onClose: () => void;
}
/**
 * Preferences, saved immediately.
 *
 * There is no Save button on purpose: every control writes straight through to
 * `%APPDATA%\VYNX\QR\settings.json`, so there is no state to lose by closing.
 */
export function SettingsModal({
  settings,
  systemInfo,
  onChange,
  onClose,
}: SettingsModalProps): JSX.Element {
  const patch = (next: Partial<Settings>): void => onChange({ ...settings, ...next });

  return (
    <Modal
      title="Settings"
      onClose={onClose}
      footer={
        <Button variant="subtle" onClick={onClose}>
          Close
        </Button>
      }
    >
      <section className={ui.section}>
        <h3 className={ui.sectionTitle}>Appearance</h3>
        <p className={ui.sectionDescription}>
          Windows owns the window frame; these settings change the app inside it.
        </p>

        <div className={ui.row}>
          <span className={ui.fieldLabel}>Theme</span>
          <Segmented
            label="Theme"
            value={settings.theme}
            onChange={(theme: ThemeMode) => patch({ theme })}
            options={[
              { value: 'system', label: 'System' },
              { value: 'light', label: 'Light' },
              { value: 'dark', label: 'Dark' },
            ]}
          />
        </div>

        <Toggle
          label="Use the Windows accent colour"
          description="Falls back to VYNX blue when Windows reports no custom accent."
          checked={settings.useWindowsAccent}
          onChange={(useWindowsAccent) => patch({ useWindowsAccent })}
        />

        <InfoNotice>
          {systemInfo
            ? `Running on Windows build ${systemInfo.osBuild}${
                systemInfo.isWindows11 ? ' (Windows 11 or newer)' : ''
              }.`
            : 'Host details are unavailable.'}
        </InfoNotice>
      </section>

      <section className={ui.section}>
        <h3 className={ui.sectionTitle}>Clipboard</h3>
        <p className={ui.sectionDescription}>
          VYNX QR reads the clipboard once at start-up and never again.
        </p>
        <Toggle
          label="Offer clipboard content on start-up"
          checked={settings.clipboardCheck}
          onChange={(clipboardCheck) => patch({ clipboardCheck })}
        />
        <Toggle
          label="Use clipboard content immediately"
          description="Skip the suggestion and encode whatever was copied."
          checked={settings.autoPaste}
          onChange={(autoPaste) => patch({ autoPaste })}
        />
      </section>

      <section className={ui.section}>
        <h3 className={ui.sectionTitle}>Export defaults</h3>

        <Field label="Format">
          {({ id, describedBy }) => (
            <select
              className={ui.select}
              id={id}
              aria-describedby={describedBy}
              value={settings.defaultFormat}
              onChange={(event) =>
                patch({ defaultFormat: event.target.value as Settings['defaultFormat'] })
              }
            >
              <option value="png">PNG image</option>
              <option value="svg">SVG vector</option>
            </select>
          )}
        </Field>

        <Field label="Size">
          {({ id, describedBy }) => (
            <select
              className={ui.select}
              id={id}
              aria-describedby={describedBy}
              value={settings.defaultSize}
              onChange={(event) => patch({ defaultSize: Number(event.target.value) })}
            >
              {SIZES.map((size) => (
                <option key={size} value={size}>
                  {size} × {size}
                </option>
              ))}
            </select>
          )}
        </Field>

        <div className={ui.row}>
          <span className={ui.fieldLabel}>Error correction</span>
          <Segmented
            label="Error correction level"
            value={settings.defaultErrorCorrection}
            onChange={(defaultErrorCorrection: EcLevel) => patch({ defaultErrorCorrection })}
            options={[
              { value: 'l', label: 'L' },
              { value: 'm', label: 'M' },
              { value: 'q', label: 'Q' },
              { value: 'h', label: 'H' },
            ]}
          />
        </div>

        <div className={ui.row}>
          <span className={ui.fieldLabel}>Module shape</span>
          <Segmented
            label="Default module shape"
            value={settings.defaultModuleStyle}
            onChange={(defaultModuleStyle: ModuleStyle) => patch({ defaultModuleStyle })}
            options={[
              { value: 'square', label: 'Square' },
              { value: 'rounded', label: 'Rounded' },
            ]}
          />
        </div>
      </section>
    </Modal>
  );
}
