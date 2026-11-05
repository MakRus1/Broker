import { expect, it } from 'vitest';
import { destinationPosition } from './workspaceScroll';
const previous = {
  y: 900,
  anchor: { kind: 'field' as const, text: 'Address', occurrence: 0, top: 120 },
};
const saved = { y: 300 };
it('restores an independent position when returning to a workspace or inner tab', () => {
  expect(destinationPosition(saved, previous, false, false)).toBe(saved);
  expect(destinationPosition(saved, previous, true, false)).toBe(saved);
});
it('keeps the edited block after saving or cancelling instead of an older view position', () => {
  expect(destinationPosition(saved, previous, true, true)).toBe(previous);
});
it('keeps the current viewport for a new inner tab but starts a new workspace at the top', () => {
  expect(destinationPosition(undefined, previous, true, false)).toBe(previous);
  expect(destinationPosition(undefined, previous, false, false)).toEqual({ y: 0 });
  expect(destinationPosition(undefined, undefined, true, true)).toEqual({ y: 0 });
});
