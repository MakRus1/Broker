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
