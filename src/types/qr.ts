/**
 * Types shared with the Rust core.
 *
 * Every shape here has an exact counterpart in `src-tauri/src`. They are written
 * by hand rather than generated so the compiler catches drift immediately.
 */

export type WifiSecurity = 'wpa' | 'wep' | 'none';

export type EcLevel = 'l' | 'm' | 'q' | 'h';

export type ModuleStyle = 'square' | 'rounded';

export type ContentKind = 'url' | 'email' | 'phone' | 'text';

export type QrPayload =
  | { type: 'text'; text: string }
  | { type: 'url'; url: string }
  | { type: 'wifi'; ssid: string; password: string; security: WifiSecurity; hidden: boolean }
  | {
      type: 'vCard';
      firstName: string;
      lastName: string;
      organization: string;
      jobTitle: string;
      phone: string;
      email: string;
      website: string;
      address: string;
      note: string;
    }
  | { type: 'email'; to: string; subject: string; body: string }
  | { type: 'phone'; number: string }
  | { type: 'sms'; number: string; message: string }
  | { type: 'geo'; latitude: number; longitude: number; label: string };

export interface Analysis {
  input: string;
  kind: ContentKind;
  payload: QrPayload;
  encoded: string;
  normalization: string | null;
  notice: string | null;
}
export interface LogoInput {
  name: string;
  /** Base64 encoded PNG, JPEG or WebP bytes. */
  data: string;
}
export interface QrStyle {
  moduleStyle: ModuleStyle;
  foreground: string;
  background: string;
  quietZone: number;
  logoRatio: number;
  logo: LogoInput | null;
}
export interface RenderRequest {
  payload: QrPayload;
  style: QrStyle;
  ecLevel: EcLevel;
  sizePx: number;
}
export type VerifyStatus = 'verified' | 'failed' | 'mismatch';

export interface Verification {
  status: VerifyStatus;
  decoded: string | null;
  reduced: VerifyStatus | null;
}
export interface QrWarning {
  code: string;
  message: string;
}
export interface RenderResult {
  pngBase64: string;
  width: number;
  height: number;
  modules: number;
  totalModules: number;
  quietZone: number;
  version: number;
  ecLevel: EcLevel;
  ecAdjusted: boolean;
  contrast: number;
  verification: Verification;
  warnings: QrWarning[];
  encoded: string;
}
export type ExportFormat = 'png' | 'svg';

export interface LogoAssetPayload {
  name: string;
  data: string;
  width: number;
  height: number;
}
export interface SystemInfo {
  osBuild: string;
  /** Windows 11 and newer start at build 22000. */
  isWindows11: boolean;
  accentColor: string | null;
  development: boolean;
}
/** Payload types that have a dedicated form. */
export type SpecialKind = Exclude<QrPayload['type'], 'text' | 'url'>;
