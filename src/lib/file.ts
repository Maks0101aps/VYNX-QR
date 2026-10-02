import type { LogoInput } from '@/types/qr';

/** Extensions the logo picker offers, matching the Rust decoder exactly. */
export const LOGO_EXTENSIONS = ['png', 'jpg', 'jpeg', 'webp'] as const;

/**
 * Build a {@link LogoInput} from a browser `File`.
 *
 * The base64 is the raw file bytes with no `data:` prefix, which is exactly what
 * the Rust `LogoInput` expects.
 */
export async function fileToLogoInput(file: File): Promise<LogoInput> {
  const data = await readAsBase64(file);
  return { name: file.name, data };
}

function readAsBase64(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(new Error('The image could not be read.'));
    reader.onload = () => {
      const result = reader.result;
      if (typeof result !== 'string') {
        reject(new Error('The image could not be read.'));
        return;
      }
      resolve(result.slice(result.indexOf(',') + 1));
    };
    reader.readAsDataURL(file);
  });
}
