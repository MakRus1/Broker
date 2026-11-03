import { expect, it } from 'vitest';
import { fitRegistryWidths } from './registryColumnWidths';

it('fits the standard shipment layout without a needless scrollbar', () => {
  const widths = fitRegistryWidths([180, 190, 280, 340, 180], 1000);
  expect(widths.reduce((sum, width) => sum + width, 0)).toBeCloseTo(1000);
  expect(widths.every((width) => width >= 120)).toBe(true);
  expect(widths[3]).toBeGreaterThan(widths[0]);
});

it('keeps readable minimum widths when the screen is too narrow', () => {
  expect(fitRegistryWidths([250, 180, 480], 300)).toEqual([120, 120, 120]);
});

it('leaves fitting widths unchanged, including before the first measurement', () => {
  expect(fitRegistryWidths([250, 180], 700)).toEqual([250, 180]);
  expect(fitRegistryWidths([250, 180], 0)).toEqual([250, 180]);
});
