import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
const tokens = readFileSync(
  new URL('../../../../shared/theme/tokens.css', import.meta.url),
  'utf8',
);

function luminance(color) {
  const channels = [1, 3, 5]
    .map((at) => parseInt(color.slice(at, at + 2), 16) / 255)
    .map((value) => (value <= 0.04045 ? value / 12.92 : ((value + 0.055) / 1.055) ** 2.4));
  return channels[0] * 0.2126 + channels[1] * 0.7152 + channels[2] * 0.0722;
}
describe('контраст основных элементов оформления', () => {
  const palette = {};
  for (const [index, block] of [...tokens.matchAll(/\{([^}]+)\}/g)].entries()) {
    for (const match of block[1].matchAll(/--([\w-]+):\s*(#[\da-f]+);/g))
      palette[match[1]] = match[2];
    const colors = { ...palette };
    for (const [foreground, background] of [
      ['text', 'surface'],
      ['text-secondary', 'surface'],
      ['text-muted', 'surface'],
      ['accent-text', 'surface'],
      ['on-accent', 'accent'],
      ['on-accent', 'danger-fill'],
      ['danger', 'danger-soft'],
      ['warning', 'warning-soft'],
      ['success', 'success-soft'],
    ]) {
      it(`${index === 0 ? 'светлая' : 'тёмная'}: ${foreground} / ${background}`, () => {
        const [low, high] = [luminance(colors[foreground]), luminance(colors[background])].sort(
          (a, b) => a - b,
        );
        expect((high + 0.05) / (low + 0.05)).toBeGreaterThanOrEqual(4.5);
      });
    }
  }
});
