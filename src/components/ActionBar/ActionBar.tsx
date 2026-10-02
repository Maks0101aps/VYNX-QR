import { useEffect, useRef, useState, type JSX } from 'react';

import type { ExportFormat } from '@/types/qr';
import { Button } from '@/components/ui/Button';
import { MenuItem, Popover } from '@/components/ui/Popover';
import styles from './ActionBar.module.css';

interface ActionBarProps {
  disabled: boolean;
  copying: boolean;
  saving: boolean;
  onCopy: () => void;
  onSave: (format: ExportFormat) => void;
  onCustomize: () => void;
  customizeOpen: boolean;
}

interface FormatOption {
  format: ExportFormat;
  glyph: string;
  label: string;
  hint: string;
}

const FORMATS: readonly FormatOption[] = [
  { format: 'png', glyph: '▦', label: 'PNG image', hint: 'Raster, any size' },
  { format: 'svg', glyph: '◈', label: 'SVG vector', hint: 'Scales without quality loss' },
];

/**
 * Copy, Save and Customize. Copy writes a real bitmap to the Windows clipboard,
 * not a file path.
 */
export function ActionBar({
  disabled,
  copying,
  saving,
  onCopy,
  onSave,
  onCustomize,
  customizeOpen,
}: ActionBarProps): JSX.Element {
  const [saveOpen, setSaveOpen] = useState(false);
  const [activeIndex, setActiveIndex] = useState(0);
  const saveRef = useRef<HTMLButtonElement>(null);

  useEffect(() => {
    if (disabled || saving) setSaveOpen(false);
  }, [disabled, saving]);

  return (
    <div className={styles.wrap}>
      <Button
        variant="primary"
        className={styles.copied}
        disabled={disabled || copying}
        onClick={onCopy}
        aria-label="Copy QR code image to the clipboard"
      >
        {copying ? '✓ Copied' : 'Copy'}
      </Button>

      <Button
        ref={saveRef}
        disabled={disabled || saving}
        onClick={() => setSaveOpen((open) => !open)}
        aria-haspopup="menu"
        aria-expanded={saveOpen}
        aria-label="Save QR code"
      >
        {saving ? 'Saving…' : 'Save ▾'}
      </Button>

      <Button onClick={onCustomize} aria-pressed={customizeOpen} aria-label="Customize the QR code">
        Customize
      </Button>

      {saveOpen ? (
        <Popover anchor={saveRef.current} align="end" onClose={() => setSaveOpen(false)}>
          {FORMATS.map((item, index) => (
            <MenuItem
              key={item.format}
              glyph={item.glyph}
              label={item.label}
              description={item.hint}
              active={index === activeIndex}
              onActive={() => setActiveIndex(index)}
              onSelect={() => {
                setSaveOpen(false);
                onSave(item.format);
              }}
            />
          ))}
        </Popover>
      ) : null}
    </div>
  );
}
