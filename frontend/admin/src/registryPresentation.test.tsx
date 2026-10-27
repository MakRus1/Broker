import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import { EnabledFilter, RegistryFilters, SortHeading } from './components/Registry';

describe('управление фильтрами таблиц', () => {
  it('показывает общее действие после всех условий и оформленный комбобокс', () => {
    const html = renderToStaticMarkup(
      <RegistryFilters
        value={{ sort: 'name', descending: false }}
        onApply={() => {}}
        placeholder="Название…"
      >
        {() => <EnabledFilter value={false} onChange={() => {}} />}
      </RegistryFilters>,
    );
    expect(html).toContain('Применить фильтры');
    expect(html.indexOf('Применить фильтры')).toBeGreaterThan(html.indexOf('Состояние'));
    expect(html).toContain('role="combobox"');
    expect(html).toContain('Отключённые');
    expect(html).not.toContain('<select');
  });
  it('указывает направление сортировки на заголовке столбца', () => {
    const html = renderToStaticMarkup(
      <table>
        <thead>
          <tr>
            <SortHeading
              label="Организация"
              field="name"
              value={{ sort: 'name', descending: true }}
              onChange={() => {}}
            />
          </tr>
        </thead>
      </table>,
    );
    expect(html).toContain('aria-sort="descending"');
    expect(html).toContain('Организация');
  });
});
