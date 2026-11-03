import { CalendarDate } from '../dates/CalendarDate';

export function restoreDraft<T extends object>(
  raw: unknown,
  dateFields: readonly string[] = [],
): T {
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) throw new Error('Invalid draft');
  const value = { ...raw } as Record<string, unknown>;
  for (const key of dateFields) {
    if (value[key] !== null && typeof value[key] !== 'string')
      throw new Error('Invalid draft date');
    value[key] = value[key] === null ? null : CalendarDate.fromISO(value[key] as string);
  }
  return value as T;
}
