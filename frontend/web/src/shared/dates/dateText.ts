// UI принимает привычный формат; API по-прежнему получает ISO без смены часового пояса.
export function parseDateInput(text: string, withTime = false): string | undefined {
  const raw = text.trim();
  const input =
    /^\d{8}$/.test(raw) && !withTime
      ? `${raw.slice(0, 2)}.${raw.slice(2, 4)}.${raw.slice(4)}`
      : /^\d{12}$/.test(raw) && withTime
        ? `${raw.slice(0, 2)}.${raw.slice(2, 4)}.${raw.slice(4, 8)} ${raw.slice(8, 10)}:${raw.slice(10)}`
        : raw;
  if (!input) return '';
  const match = withTime
    ? /^(\d{2})\.(\d{2})\.(\d{4})[ T](\d{2}):(\d{2})$/.exec(input)
    : /^(\d{2})\.(\d{2})\.(\d{4})$/.exec(input);
  const iso = withTime
    ? /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})$/.exec(input)
    : /^(\d{4})-(\d{2})-(\d{2})$/.exec(input);
  if (!match && !iso) return undefined;
  const [year, month, day] = iso
    ? iso.slice(1, 4).map(Number)
    : [Number(match![3]), Number(match![2]), Number(match![1])];
  const hour = Number((iso ?? match)?.[4] ?? 0);
  const minute = Number((iso ?? match)?.[5] ?? 0);
  const leap = year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0);
  const days = [31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
  if (
    year < 1 ||
    month < 1 ||
    month > 12 ||
    day < 1 ||
    day > days[month - 1] ||
    hour > 23 ||
    minute > 59
  )
    return undefined;
  const pad = (value: number) => String(value).padStart(2, '0');
  return `${String(year).padStart(4, '0')}-${pad(month)}-${pad(day)}${withTime ? `T${pad(hour)}:${pad(minute)}` : ''}`;
}

export function displayDateInput(value: string) {
  if (!value) return '';
  const [date, time] = value.split('T');
  return date.split('-').reverse().join('.') + (time ? ` ${time.slice(0, 5)}` : '');
}

export function maskDateInput(raw: string, cursor: number, withTime = false, deleting = false) {
  // ISO вставляется целиком, поэтому сначала переводим его в экранный формат.
  const iso = /^\d{4}-\d{2}-\d{2}(?:T\d{2}:\d{2})?$/.test(raw.trim());
  if (iso && parseDateInput(raw, withTime) !== undefined) {
    const text = displayDateInput(raw.trim());
    return { text, cursor: text.length };
  }
  // Произвольный текст остаётся виден и валидируется, а не превращается молча в дату.
  if (/[^\d. :]/.test(raw)) return { text: raw, cursor };
  // Уже разделённые части не склеиваем: «11.1.2026» — это неполный месяц,
  // а не поток цифр, из которого можно забирать цифры года.
  if (raw.includes('.')) {
    const boundaries = withTime
      ? [/^\d{2}\.\d{2}$/, /^\d{2}\.\d{2}\.\d{4}$/, /^\d{2}\.\d{2}\.\d{4} \d{2}$/]
      : [/^\d{2}\.\d{2}$/];
    const separators = withTime ? ['.', ' ', ':'] : ['.'];
    const boundary = boundaries.findIndex((pattern) => pattern.test(raw));
    if (!deleting && cursor === raw.length && boundary >= 0)
      return { text: raw + separators[boundary], cursor: cursor + 1 };
    const parts = raw.slice(0, cursor).split(/[. :]/);
    const widths = [2, 2, 4, 2, 2];
    const completed = parts.at(-1)?.length === widths[parts.length - 1];
    const skip = !deleting && completed && /[. :]/.test(raw[cursor] ?? '');
    return { text: raw, cursor: cursor + (skip ? 1 : 0) };
  }
  const digits = raw.replace(/\D/g, '').slice(0, withTime ? 12 : 8);
  const beforeCursor = raw.slice(0, cursor).replace(/\D/g, '').length;
  const separators: Record<number, string> = withTime
    ? { 2: '.', 4: '.', 8: ' ', 10: ':' }
    : { 2: '.', 4: '.' };
  let text = '';
  let nextCursor = 0;
  for (let index = 0; index < digits.length; index++) {
    text += digits[index];
    if (index < beforeCursor) nextCursor = text.length;
    const boundary = separators[index + 1];
    if (boundary && (index + 1 < digits.length || !deleting)) {
      text += boundary;
      if (index + 1 === beforeCursor && cursor === raw.length && !deleting)
        nextCursor = text.length;
    }
  }
  return { text, cursor: nextCursor };
}
