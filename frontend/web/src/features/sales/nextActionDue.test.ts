import { describe, expect, it } from 'vitest';
import { nextActionDue } from './nextActionDue';
describe('next action deadlines', () => {
  const now = new Date(2026, 10, 8, 23, 59);
  it('distinguishes overdue, today, approaching and later dates', () => {
    expect(nextActionDue('2026-11-07', now)).toBe('overdue');
    expect(nextActionDue('2026-11-08', now)).toBe('today');
    expect(nextActionDue('2026-11-11', now)).toBe('soon');
    expect(nextActionDue('2026-11-12', now)).toBeUndefined();
    expect(nextActionDue(undefined, now)).toBeUndefined();
  });
  it('uses calendar days across month and year boundaries', () => {
    expect(nextActionDue('2027-01-01', new Date(2026, 11, 31, 23))).toBe('soon');
    expect(nextActionDue('2026-11-08', new Date(2026, 10, 9))).toBe('overdue');
  });
});
