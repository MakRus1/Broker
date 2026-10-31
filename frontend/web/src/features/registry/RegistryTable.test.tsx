import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it } from 'vitest';
import { RegistryTable, type RegistryColumn } from './RegistryTable';
import { restoreColumns } from './registryPreferences';

it('сохраняет соответствие заголовков и ячеек после скрытия и перестановки', () => {
  const definitions: RegistryColumn<{ id: string; name: string; country: string }>[] = [
    {
      id: 'name',
      label: 'Имя',
      width: 250,
      required: true,
      sort: 'name',
      render: (item) => item.name,
    },
    { id: 'country', label: 'Страна', width: 180, render: (item) => item.country },
    { id: 'hidden', label: 'Скрытый', width: 180, render: () => 'Секретная ячейка' },
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
      columns={{ columns, definitions, setColumns: () => {} }}
      sort="name"
      descending={true}
      onSort={() => {}}
      onOpen={() => {}}
    />,
  );
  expect(html).not.toContain('Секретная ячейка');
  expect(html.indexOf('Страна')).toBeLessThan(html.indexOf('Имя'));
  expect(html.indexOf('<td>RU')).toBeLessThan(html.indexOf('<td>Компания'));
  expect(html).toContain('aria-sort="descending"');
  expect(html).toContain('aria-valuenow="220"');
});
