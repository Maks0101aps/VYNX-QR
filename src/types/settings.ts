import type { EcLevel, ModuleStyle } from './qr';

export type ThemeMode = 'system' | 'light' | 'dark';

export type ExportFormat = 'png' | 'svg';

export interface Settings {
  theme: ThemeMode;
  useWindowsAccent: boolean;
  clipboardCheck: boolean;
  autoPaste: boolean;
  defaultFormat: ExportFormat;
  defaultSize: number;
  defaultErrorCorrection: EcLevel;
  defaultModuleStyle: ModuleStyle;
}
export const DEFAULT_SETTINGS: Settings = {
  theme: 'system',
  useWindowsAccent: true,
  clipboardCheck: true,
  autoPaste: false,
  defaultFormat: 'png',
  defaultSize: 1024,
  // High keeps every payload type comfortably scannable at a modest size cost.
  defaultErrorCorrection: 'h',
  defaultModuleStyle: 'square',
};
