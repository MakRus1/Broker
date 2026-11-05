import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it } from 'vitest';
import { RegistryTable, type RegistryColumn } from './RegistryTable';
import { restoreColumns } from './registryPreferences';

it('сохраняет соответствие заголовков и ячеек после скрытия и перестановки', () => {
  const definitions: RegistryColumn<{ id: string; name: string; country: string }>[] = [
    {
      id: 'name',
      labelKey: 'registryField.name' as const,
      width: 250,
      required: true,
      sort: 'name',
      render: (item) => item.name,
    },
    {
      id: 'country',
      labelKey: 'registryField.country_code' as const,
      width: 180,
      render: (item) => item.country,
    },
    {
      id: 'hidden',
      labelKey: 'registryField.archived' as const,
      width: 180,
      render: () => 'Секретная ячейка',
    },
  ];
  const columns = restoreColumns(
    [
      { id: 'country', visible: true, width: 220 },
      { id: 'hidden', visible: false, width: 180 },
    ],
    definitions,
  );
  const html = renderToStaticMarkup(
    <RegistryTable
      items={[{ id: '1', name: 'Компания', country: 'RU' }]}
      definitions={definitions}
      columns={{
        columns,
        definitions,
        setColumns: () => {},
        newColumns: [],
        acknowledgeColumns: () => {},
        guidanceKey: '',
      }}
      sort="name"
      descending={true}
      onSort={() => {}}
      onOpen={() => {}}
    />,
  );
  expect(html).not.toContain('Секретная ячейка');
  expect(html.indexOf('Страна')).toBeLessThan(html.indexOf('Наименование'));
  expect(html.indexOf('<td>RU')).toBeLessThan(html.indexOf('<td>Компания'));
  expect(html).toContain('aria-sort="descending"');
  expect(html).toContain('aria-valuenow="220"');
});

it('ограничивает DOM больших реестров и сохраняет логическое число строк', () => {
  const definitions = [
    {
      id: 'name',
      labelKey: 'registryField.name' as const,
      width: 160,
      render: (item: { id: string }) => item.id,
    },
  ];
  const items = Array.from({ length: 1000 }, (_, index) => ({ id: String(index) }));
  const columns = {
    columns: restoreColumns(null, definitions),
    definitions,
    setColumns: () => {},
    newColumns: [],
    acknowledgeColumns: () => {},
    guidanceKey: '',
  };
  const html = renderToStaticMarkup(
    <RegistryTable items={items} definitions={definitions} columns={columns} onOpen={() => {}} />,
  );
  expect(html).toContain('aria-rowcount="1001"');
  expect(html).toContain('aria-rowindex="2"');
  expect((html.match(/data-virtual-index=/g) ?? []).length).toBeLessThan(30);
  const full = renderToStaticMarkup(
    <RegistryTable
      virtualize={false}
      items={items}
      definitions={definitions}
      columns={columns}
      onOpen={() => {}}
    />,
  );
  expect((full.match(/<tr/g) ?? []).length).toBe(1001);
});

it('виртуализирует широкую таблицу по обеим осям и сохраняет логические индексы', () => {
  const definitions = Array.from({ length: 100 }, (_, index) => ({
    id: `c${index}`,
    labelKey: 'registryField.name' as const,
    width: 160,
    render: (item: { id: string }) => `${item.id}:${index}`,
  }));
  const html = renderToStaticMarkup(
    <RegistryTable
      items={Array.from({ length: 100 }, (_, index) => ({ id: String(index) }))}
      definitions={definitions}
      columns={{
        columns: restoreColumns(null, definitions),
        definitions,
        setColumns: () => {},
        newColumns: [],
        acknowledgeColumns: () => {},
        guidanceKey: '',
      }}
      onOpen={() => {}}
      selectionColumn={{ header: 'Выбор', render: () => <input type="checkbox" /> }}
    />,
  );
  expect(html).toContain('aria-colcount="101"');
  expect(html).toContain('aria-colindex="2"');
  expect(html).toContain('aria-rowcount="101"');
  expect(html).toContain('registry-column-spacer');
  expect((html.match(/data-column=/g) ?? []).length).toBeLessThan(16);
  expect((html.match(/<td/g) ?? []).length).toBeLessThan(500);
});

it('сохраняет позиции выгруженных строк без вызова рендера отсутствующих данных', () => {
  const definitions: RegistryColumn<{ id: string; name: string }>[] = [
    {
      id: 'name',
      labelKey: 'registryField.name',
      width: 200,
      render: (item) => item.name,
    },
  ];
  const html = renderToStaticMarkup(
    <RegistryTable
      items={[]}
      pageSource={{
        rows: [{ id: '1' }, { id: '2' }],
        get: (i) => (i === 1 ? { id: '2', name: 'Вторая' } : undefined),
        ensure: () => {},
        failed: () => false,
        retry: () => {},
      }}
      definitions={definitions}
      columns={{
        columns: restoreColumns(null, definitions),
        definitions,
        setColumns: () => {},
        newColumns: [],
        acknowledgeColumns: () => {},
        guidanceKey: '',
      }}
      onOpen={() => {}}
      emptyMessage="Пустой реестр"
    />,
  );
  expect(html).toContain('registry-page-placeholder');
  expect(html).toContain('Вторая');
  expect(html).not.toContain('Пустой реестр');
});
