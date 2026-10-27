import { parseDateInput } from './dateText';

// Календарный день без времени и часового пояса. В JSON остаётся YYYY-MM-DD.
export class CalendarDate {
  private constructor(
    readonly year: number,
    readonly month: number,
    readonly day: number,
  ) {}

  static fromISO(value: string): CalendarDate {
    if (!/^\d{4}-\d{2}-\d{2}$/.test(value) || parseDateInput(value) === undefined)
      throw new Error('Некорректная календарная дата');
    const [year, month, day] = value.split('-').map(Number);
    return new CalendarDate(year, month, day);
  }

  toString(): string {
    return `${String(this.year).padStart(4, '0')}-${String(this.month).padStart(2, '0')}-${String(this.day).padStart(2, '0')}`;
  }

  toDisplayString(): string {
    return `${String(this.day).padStart(2, '0')}.${String(this.month).padStart(2, '0')}.${String(this.year).padStart(4, '0')}`;
  }

  toJSON(): string {
    return this.toString();
  }
}
