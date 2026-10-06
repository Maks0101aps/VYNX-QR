/**
 * Generates the VYNX QR icon set from a single 1024x1024 master image.
 *
 * The master is drawn procedurally with signed distance fields so it stays
 * crisp at every size and the repository needs no binary design tooling.
 * Run with: npm run icons
 */
import { deflateSync } from 'node:zlib';
import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const ICONS_DIR = resolve(HERE, '..', 'app', 'resources', 'icons');

/* ------------------------------------------------------------------ PNG ---- */

const CRC_TABLE = (() => {
  const table = new Int32Array(256);
  for (let n = 0; n < 256; n += 1) {
    let c = n;
    for (let k = 0; k < 8; k += 1) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    table[n] = c;
  }
  return table;
})();

function crc32(buffer) {
  let crc = -1;
  for (const byte of buffer) crc = CRC_TABLE[(crc ^ byte) & 0xff] ^ (crc >>> 8);
  return (crc ^ -1) >>> 0;
}

function pngChunk(type, data) {
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length, 0);
  const body = Buffer.concat([Buffer.from(type, 'ascii'), data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body), 0);
  return Buffer.concat([length, body, crc]);
}

function encodePng(width, height, rgba) {
  const stride = width * 4;
  const raw = Buffer.alloc((stride + 1) * height);
  for (let y = 0; y < height; y += 1) {
    raw[y * (stride + 1)] = 0; // filter: none
    rgba.copy(raw, y * (stride + 1) + 1, y * stride, (y + 1) * stride);
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0);
  ihdr.writeUInt32BE(height, 4);
  ihdr[8] = 8; // bit depth
  ihdr[9] = 6; // RGBA
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    pngChunk('IHDR', ihdr),
    pngChunk('IDAT', deflateSync(raw, { level: 9 })),
    pngChunk('IEND', Buffer.alloc(0)),
  ]);
}

/* --------------------------------------------------------------- shapes ---- */

const clamp = (value, min, max) => Math.min(max, Math.max(min, value));
const mix = (a, b, t) => a + (b - a) * t;

/** Signed distance to a rounded box centred on the origin. */
function sdRoundedBox(px, py, halfWidth, halfHeight, radius) {
  const qx = Math.abs(px) - halfWidth + radius;
  const qy = Math.abs(py) - halfHeight + radius;
  const outside = Math.hypot(Math.max(qx, 0), Math.max(qy, 0));
  return outside + Math.min(Math.max(qx, qy), 0) - radius;
}

/** Signed distance to a thick line segment (a capsule). */
function sdSegment(px, py, ax, ay, bx, by, radius) {
  const vx = bx - ax;
  const vy = by - ay;
  const wx = px - ax;
  const wy = py - ay;
  const t = clamp((wx * vx + wy * vy) / (vx * vx + vy * vy), 0, 1);
  return Math.hypot(wx - vx * t, wy - vy * t) - radius;
}

/** Coverage from a distance, approximated over one pixel of edge softness. */
function coverage(distance) {
  return clamp(0.5 - distance, 0, 1);
}

function over(dst, index, r, g, b, alpha) {
  const inv = 1 - alpha;
  dst[index] = Math.round(r * alpha + dst[index] * inv);
  dst[index + 1] = Math.round(g * alpha + dst[index + 1] * inv);
  dst[index + 2] = Math.round(b * alpha + dst[index + 2] * inv);
  dst[index + 3] = Math.round(255 * alpha + dst[index + 3] * inv);
}

/* ----------------------------------------------------------------- mark ---- */

const ACCENT_TOP = [0x4c, 0x8d, 0xff];
const ACCENT_BOTTOM = [0x1d, 0x4e, 0xd8];
const WHITE = [0xff, 0xff, 0xff];

/**
 * Draw the VYNX mark: a rounded app tile carrying a bold "V" with a small QR
 * finder pattern in the lower right corner. The V stays legible at 16 px, the
 * finder pattern adds the QR identity from 32 px upwards.
 */
function drawMark(size) {
  const pixels = Buffer.alloc(size * size * 4, 0);
  const s = (value) => (value / 1024) * size;

  // Geometry expressed in the 1024 unit master space.
  const tileHalf = 512;
  const tileRadius = s(224);
  const strokeRadius = s(74);
  const vLeft = { x: s(250), y: s(292) };
  const vBottom = { x: s(506), y: s(752) };
  const vRight = { x: s(762), y: s(292) };
  const finderHalf = s(92);
  const finderCentre = { x: s(744), y: s(744) };
  const finderHoleHalf = finderHalf - s(30);
  const finderDotHalf = s(26);

  for (let y = 0; y < size; y += 1) {
    for (let x = 0; x < size; x += 1) {
      const index = (y * size + x) * 4;
      const cx = x + 0.5;
      const cy = y + 0.5;

      const tile = sdRoundedBox(cx - s(512), cy - s(512), tileHalf, tileHalf, tileRadius);
      const tileAlpha = coverage(tile);
      if (tileAlpha <= 0) continue;

      const t = y / size;
      const base = [
        Math.round(mix(ACCENT_TOP[0], ACCENT_BOTTOM[0], t)),
        Math.round(mix(ACCENT_TOP[1], ACCENT_BOTTOM[1], t)),
        Math.round(mix(ACCENT_TOP[2], ACCENT_BOTTOM[2], t)),
      ];
      over(pixels, index, base[0], base[1], base[2], tileAlpha);

      // Subtle top highlight so the tile does not look flat at large sizes.
      const highlight = clamp(1 - cy / s(340), 0, 1) * 0.1;
      if (highlight > 0) {
        over(pixels, index, 255, 255, 255, tileAlpha * highlight);
      }

      // The V.
      const left = sdSegment(cx, cy, vLeft.x, vLeft.y, vBottom.x, vBottom.y, strokeRadius);
      const right = sdSegment(cx, cy, vBottom.x, vBottom.y, vRight.x, vRight.y, strokeRadius);
      const glyph = Math.min(left, right);
      over(pixels, index, WHITE[0], WHITE[1], WHITE[2], tileAlpha * coverage(glyph));

      // QR finder mark, lower right.
      const dx = cx - finderCentre.x;
      const dy = cy - finderCentre.y;
      const outer = sdRoundedBox(dx, dy, finderHalf, finderHalf, s(22));
      const hole = sdRoundedBox(dx, dy, finderHoleHalf, finderHoleHalf, s(12));
      const dot = sdRoundedBox(dx, dy, finderDotHalf, finderDotHalf, s(8));
      const ring = Math.max(outer, -hole);
      const mark = Math.min(ring, dot);
      over(pixels, index, WHITE[0], WHITE[1], WHITE[2], tileAlpha * coverage(mark));
    }
  }
  return pixels;
}

/* ------------------------------------------------------------------ BMP ---- */

/** 24 bit uncompressed BMP, bottom-up, as NSIS expects for installer art. */
function encodeBmp(width, height, rgb) {
  const rowSize = Math.ceil((width * 3) / 4) * 4;
  const pixelBytes = Buffer.alloc(rowSize * height);
  for (let y = 0; y < height; y += 1) {
    const src = (height - 1 - y) * width * 3;
    const dst = y * rowSize;
    rgb.copy(pixelBytes, dst, src, src + width * 3);
  }
  const header = Buffer.alloc(54);
  header.write('BM', 0, 'ascii');
  header.writeUInt32LE(54 + pixelBytes.length, 2);
  header.writeUInt32LE(54, 10);
  header.writeUInt32LE(40, 14);
  header.writeInt32LE(width, 18);
  header.writeInt32LE(height, 22);
  header.writeUInt16LE(1, 26);
  header.writeUInt16LE(24, 28);
  header.writeUInt32LE(pixelBytes.length, 34);
  header.writeInt32LE(2835, 38);
  header.writeInt32LE(2835, 42);
  return Buffer.concat([header, pixelBytes]);
}

function gradientBmp(width, height, from, to) {
  const rgb = Buffer.alloc(width * height * 3);
  for (let y = 0; y < height; y += 1) {
    const t = y / Math.max(1, height - 1);
    for (let x = 0; x < width; x += 1) {
      const index = (y * width + x) * 3;
      rgb[index] = Math.round(mix(from[0], to[0], t));
      rgb[index + 1] = Math.round(mix(from[1], to[1], t));
      rgb[index + 2] = Math.round(mix(from[2], to[2], t));
    }
  }
  return encodeBmp(width, height, rgb);
}

/* ----------------------------------------------------------------- ICO ---- */

/**
 * One ICO entry.
 *
 * PNG compression is only legal for the 256x256 size; every smaller size must be a
 * BITMAPINFOHEADER followed by a bottom-up BGRA bitmap and a 1bpp AND mask. Windows
 * rejects a PNG entry below 256 with ERROR_RESOURCE_TYPE_NOT_FOUND (0x80070715),
 * which is exactly what stopped the window icon from loading at runtime. So the
 * small sizes stay uncompressed, and only the largest one is compressed, because
 * that is where the 240 KB saving is.
 */
function encodeIconImage(size, rgba) {
  if (size >= 256) return encodePng(size, size, rgba);

  const header = Buffer.alloc(40);
  header.writeUInt32LE(40, 0); // biSize
  header.writeInt32LE(size, 4); // biWidth
  header.writeInt32LE(size * 2, 8); // biHeight: XOR and AND masks stacked
  header.writeUInt16LE(1, 12); // biPlanes
  header.writeUInt16LE(32, 14); // biBitCount
  header.writeUInt32LE(0, 16); // biCompression: BI_RGB

  const rowBytes = size * 4;
  const pixels = Buffer.alloc(rowBytes * size);
  for (let y = 0; y < size; y += 1) {
    // DIB rows run bottom-up.
    const source = (size - 1 - y) * rowBytes;
    for (let x = 0; x < size; x += 1) {
      const from = source + x * 4;
      const to = y * rowBytes + x * 4;
      pixels[to] = rgba[from + 2]; // B
      pixels[to + 1] = rgba[from + 1]; // G
      pixels[to + 2] = rgba[from]; // R
      pixels[to + 3] = rgba[from + 3]; // A
    }
  }

  // The alpha channel carries the transparency, so the AND mask stays all zero.
  const maskRow = Math.ceil(size / 32) * 4;
  const mask = Buffer.alloc(maskRow * size);

  return Buffer.concat([header, pixels, mask]);
}

/** Assemble the ICO container from `[{ size, image }]`. */
function encodeIco(entries) {
  const headerSize = 6 + entries.length * 16;
  const directory = Buffer.alloc(headerSize);
  directory.writeUInt16LE(0, 0); // reserved
  directory.writeUInt16LE(1, 2); // type: icon
  directory.writeUInt16LE(entries.length, 4);

  let offset = headerSize;
  entries.forEach((entry, index) => {
    const at = 6 + index * 16;
    // A dimension of 256 is stored as zero.
    directory.writeUInt8(entry.size >= 256 ? 0 : entry.size, at);
    directory.writeUInt8(entry.size >= 256 ? 0 : entry.size, at + 1);
    directory.writeUInt8(0, at + 2); // palette size
    directory.writeUInt8(0, at + 3); // reserved
    directory.writeUInt16LE(1, at + 4); // colour planes
    directory.writeUInt16LE(32, at + 6); // bits per pixel
    directory.writeUInt32LE(entry.image.length, at + 8);
    directory.writeUInt32LE(offset, at + 12);
    offset += entry.image.length;
  });

  return Buffer.concat([directory, ...entries.map((entry) => entry.image)]);
}

/* ----------------------------------------------------------------- main ---- */

mkdirSync(ICONS_DIR, { recursive: true });

const linuxDir = resolve(ICONS_DIR, 'linux');
mkdirSync(linuxDir, { recursive: true });
for (const size of [48, 64, 128, 256, 512]) {
  writeFileSync(resolve(linuxDir, `${size}x${size}.png`), encodePng(size, size, drawMark(size)));
}
if (process.argv.includes('--linux-only')) {
  console.log(`Linux icons written to ${linuxDir}`);
  process.exit(0);
}

// The same mark supplies the Windows resources and Linux hicolor icons.
const master = drawMark(1024);

// The master, kept so the set can be regenerated at any resolution.
writeFileSync(resolve(ICONS_DIR, 'source.png'), encodePng(1024, 1024, master));

// Standalone PNGs named in `bundle.icon` and imported by the title bar.
writeFileSync(resolve(ICONS_DIR, '32x32.png'), encodePng(32, 32, drawMark(32)));
writeFileSync(resolve(ICONS_DIR, '128x128.png'), encodePng(128, 128, drawMark(128)));
writeFileSync(resolve(ICONS_DIR, '128x128@2x.png'), encodePng(256, 256, drawMark(256)));

// NSIS wizard art.
writeFileSync(
  resolve(ICONS_DIR, 'installer-header.bmp'),
  gradientBmp(150, 57, ACCENT_TOP, ACCENT_BOTTOM),
);
writeFileSync(
  resolve(ICONS_DIR, 'installer-sidebar.bmp'),
  gradientBmp(164, 314, [0x27, 0x5b, 0xd6], [0x14, 0x35, 0x8c]),
);

// The icon the executable embeds and both installers display. Windows picks the
// entry matching the display it draws for, so the small sizes must be present for
// the taskbar and Explorer.
const ICO_SIZES = [16, 24, 32, 48, 64, 128, 256];
writeFileSync(
  resolve(ICONS_DIR, 'icon.ico'),
  encodeIco(ICO_SIZES.map((size) => ({ size, image: encodeIconImage(size, drawMark(size)) }))),
);

console.log(`icons written to ${ICONS_DIR}`);
