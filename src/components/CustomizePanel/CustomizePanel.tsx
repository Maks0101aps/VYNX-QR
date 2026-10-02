import type { JSX } from 'react';

import type { QrStyle, SystemInfo } from '@/types/qr';
import type { Settings } from '@/types/settings';
import { Button } from '@/components/ui/Button';
import { ColorField } from '@/components/ui/ColorField';
import { Segmented } from '@/components/ui/Segmented';
import { InfoNotice } from '@/components/ui/Toggle';
import { toPickerValue } from '@/lib/color';
import ui from '@/components/ui/ui.module.css';
import styles from '@/App.module.css';

const QUIET_ZONES = [1, 2, 3, 4, 6, 8] as const;

interface CustomizePanelProps {
  style: QrStyle;
  onChange: (style: QrStyle) => void;
  onPickLogo: () => void;
  onClearLogo: () => void;
  onClose: () => void;
  /** The Windows accent colour in effect, or `null` when VYNX blue is used. */
  accent: string | null;
  systemInfo: SystemInfo | null;
  settings: Settings;
}

/**
 * Everything that changes how the code looks, in one panel.
 *
 * Colours, module shape, quiet zone and the logo all live together because they
 * interact: a very light logo needs a bigger plate, and the renderer bumps error
 * correction to High the moment a logo is present.
 */
export function CustomizePanel({
  style,
  onChange,
  onPickLogo,
  onClearLogo,
  onClose,
  accent,
  systemInfo,
  settings,
}: CustomizePanelProps): JSX.Element {
  const patch = (next: Partial<QrStyle>): void => onChange({ ...style, ...next });
  const logoError = logoRatioTooLarge(style.logoRatio);

  return (
    <section className={styles.customize} aria-label="Customize">
      <header className={styles.customizeHeader}>
        <h2 className={styles.customizeTitle}>Customize</h2>
        <Button variant="subtle" iconOnly onClick={onClose} aria-label="Close customize panel">
          ✕
        </Button>
      </header>

      <div className={styles.field}>
        <span className={ui.fieldLabel}>Module shape</span>
        <Segmented
          label="Module shape"
          value={style.moduleStyle}
          onChange={(moduleStyle) => patch({ moduleStyle })}
          options={[
            { value: 'square', label: 'Square' },
            { value: 'rounded', label: 'Rounded' },
          ]}
          fullWidth
        />
      </div>

      <div className={styles.grid}>
        <ColorField
          label="Foreground"
          value={style.foreground}
          pickerValue={toPickerValue(style.foreground, '#FFFFFF')}
          onChange={(foreground) => patch({ foreground })}
        />
        <ColorField
          label="Background"
          value={style.background}
          pickerValue={toPickerValue(style.background, '#000000')}
          onChange={(background) => patch({ background })}
        />
      </div>

      <div className={styles.field}>
        <span className={ui.fieldLabel}>Quiet zone</span>
        <Segmented
          label="Quiet zone in modules"
          value={String(style.quietZone)}
          onChange={(value) => patch({ quietZone: Number(value) })}
          options={QUIET_ZONES.map((zone) => ({ value: String(zone), label: String(zone) }))}
          fullWidth
        />
        <span className={ui.fieldHint}>
          {style.quietZone} module{style.quietZone === 1 ? '' : 's'} of margin. The specification
          requires at least one.
        </span>
      </div>

      <div className={styles.stack}>
        <span className={ui.sectionTitle}>Logo</span>
        <div className={styles.logoRow}>
          {style.logo ? (
            <img
              className={styles.logoThumb}
              src={`data:image/png;base64,${style.logo.data}`}
              alt=""
            />
          ) : null}
          <Button onClick={onPickLogo}>Choose image…</Button>
          {style.logo ? (
            <Button variant="subtle" onClick={onClearLogo}>
              Remove
            </Button>
          ) : null}
        </div>
        <span className={styles.hint}>
          PNG, JPEG or WebP up to 2 MB. Error correction is raised to High automatically.
        </span>
        {style.logo ? (
          <div className={styles.field}>
            <span className={ui.fieldLabel}>Logo size</span>
            <input
              className={ui.input}
              type="range"
              min={5}
              max={30}
              step={1}
              aria-label="Logo size percentage"
              value={Math.round(style.logoRatio * 100)}
              onChange={(event) => patch({ logoRatio: Number(event.target.value) / 100 })}
            />
            <span className={ui.fieldHint}>{Math.round(style.logoRatio * 100)}% of the width</span>
          </div>
        ) : null}
        {logoError ? <InfoNotice tone="warning">{logoError}</InfoNotice> : null}
      </div>

      <AccentSummary accent={accent} settings={settings} systemInfo={systemInfo} />
    </section>
  );
}

function AccentSummary({
  accent,
  settings,
  systemInfo,
}: {
  accent: string | null;
  settings: Settings;
  systemInfo: SystemInfo | null;
}): JSX.Element {
  if (!settings.useWindowsAccent) {
    return <InfoNotice>VYNX blue is used for controls; the QR colours are independent.</InfoNotice>;
  }
  return (
    <InfoNotice>
      {accent
        ? `Controls follow your Windows accent (${accent}).`
        : 'Controls follow the VYNX blue: Windows reports no custom accent colour.'}
      {systemInfo?.isWindows11 ? '' : ' Windows 10 and older use Mica-free solid surfaces.'}
    </InfoNotice>
  );
}

function logoRatioTooLarge(ratio: number): string | null {
  if (ratio > 0.25) {
    return 'A logo this large can hide data modules. Try 20% or less.';
  }
  return null;
}

/** Files the picker accepts, matching the Rust decoder. */
