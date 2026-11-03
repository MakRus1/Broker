import { describe, expect, it } from 'vitest';
import { CalendarDate } from '../dates/CalendarDate';
import { restoreDraft } from './restoreDraft';

describe('restoreDraft', () => {
  it('restores calendar operations after a JSON round trip', () => {
    const source = { date: CalendarDate.fromISO('2026-11-03'), optional: null, amount: '123,45' };
    const restored = restoreDraft<typeof source>(JSON.parse(JSON.stringify(source)), [
      'date',
      'optional',
    ]);
    expect(restored.date.toDisplayString()).toBe('03.11.2026');
    expect(restored.date.addDays(30).toString()).toBe('2026-12-03');
    expect(restored.optional).toBeNull();
    expect(restored.amount).toBe('123,45');
  });
  it('rejects malformed or missing dates instead of restoring an unusable editor', () => {
    for (const date of ['2026-02-30', '03.11.2026', undefined, {}, 42]) {
      expect(() => restoreDraft({ date }, ['date'])).toThrow();
    }
    expect(() => restoreDraft(null)).toThrow();
    expect(() => restoreDraft([])).toThrow();
  });
});
