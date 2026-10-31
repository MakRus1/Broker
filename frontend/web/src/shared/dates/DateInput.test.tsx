import { describe, expect, it } from 'vitest';
import { renderToStaticMarkup } from 'react-dom/server';
import { CalendarDate } from './CalendarDate';
import { DateInput, IsoDateInput, DateTimeInput } from './DateInput';

const ignore = () => {};
describe('Контракты компонентов даты', () => {
  it('отображает типизированную дату и передаёт календарю ISO', () => {
    const html = renderToStaticMarkup(
      <DateInput value={CalendarDate.fromISO('2000-02-29')} onChange={ignore} />,
    );
    expect(html).toContain('value="29.02.2000"');
    expect(html).toContain('type="date"');
    expect(html).toContain('value="2000-02-29"');
  });
  it('пустое значение не превращается в текущую дату', () => {
    const html = renderToStaticMarkup(<DateInput value={null} onChange={ignore} />);
    expect(html).toContain('value=""');
    expect(html).not.toContain('1970-01-01');
  });
  it('адаптер DTO отображает тот же календарный день', () => {
    const html = renderToStaticMarkup(<IsoDateInput value="2026-10-25" onChange={ignore} />);
    expect(html).toContain('value="25.10.2026"');
  });
  it('дата со временем имеет отдельный компонент', () => {
    const html = renderToStaticMarkup(<DateTimeInput value="2026-10-25T09:15" onChange={ignore} />);
    expect(html).toContain('type="datetime-local"');
    expect(html).toContain('value="25.10.2026 09:15"');
  });
});
