import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it, vi } from 'vitest';
import { translate } from '../../../../shared/i18n/catalog';
import { RegistryHeading } from '../../shared/ui/RegistryHeading';
import { RegistryTable } from './RegistryTable';
import { restoreColumns } from './registryPreferences';
import { transportationColumns } from '../transportations/TransportationTable';
import { DateInput } from '../../shared/dates/DateInput';

vi.mock('../localization/Localization', () => ({
  useLocalization: () => ({
    locale: 'en',
    translate: (key: Parameters<typeof translate>[1], values?: Record<string, string | number>) =>
      translate('en', key, values),
  }),
}));

it('keeps business actions, columns and refresh together in the shared heading', () => {
  const columns = {
    definitions: transportationColumns,
    columns: restoreColumns(null, transportationColumns),
    setColumns: () => {},
    newColumns: [],
    acknowledgeColumns: () => {},
    guidanceKey: '',
  };
  const html = renderToStaticMarkup(
    <RegistryHeading title="Transportations" count={12} columns={columns} onRefresh={() => {}}>
      <button>Create</button>
    </RegistryHeading>,
  );
  expect(html).toContain('Loaded: 12');
  expect(html.indexOf('>Refresh<')).toBeLessThan(html.indexOf('>Create<'));
  expect(html).toContain('registry-heading-tools');
  expect(html).toContain('registry-heading-create');
  expect(html.indexOf('>Columns<')).toBeLessThan(html.indexOf('>Refresh<'));
  expect(html).not.toMatch(/[А-Яа-яЁё]/);
});

it('translates composite route columns and empty states', () => {
  const route = transportationColumns.find((column) => column.id === 'route')!;
  const html = renderToStaticMarkup(
    <RegistryTable
      definitions={[route]}
      columns={{
        definitions: [route],
        columns: restoreColumns(null, [route]),
        setColumns: () => {},
        newColumns: [],
        acknowledgeColumns: () => {},
        guidanceKey: '',
      }}
      items={[
        {
          id: '1',
          route_segments: [],
          revision: 1,
          shipment_id: 'shipment',
          type: 'container',
          hazardous: false,
        },
      ]}
      onOpen={() => {}}
    />,
  );
  expect(html).toContain('Route');
  expect(html).toContain('Not set');
  expect(html).not.toMatch(/[А-Яа-яЁё]/);
});

it('translates the calendar control without changing its date format', () => {
  const html = renderToStaticMarkup(<DateInput value={null} onChange={() => {}} />);
  expect(html).toContain('Open calendar');
  expect(html).toContain('DD.MM.YYYY');
  expect(html).not.toMatch(/[А-Яа-яЁё]/);
});
