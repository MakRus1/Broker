import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it, vi } from 'vitest';
import { translate } from '../../../../shared/i18n/catalog';
import { SharedRouteDetailsView, sharedRouteDirection } from '../routes/SharedRouteDetails';
import { ShipmentRoute } from './ShipmentRoute';
import type { SeaDetails } from '../routes/types';
import type { Session } from '../../shared/api/session';

vi.mock('../localization/Localization', () => ({
  useLocalization: () => ({
    locale: 'en',
    translate: (key: Parameters<typeof translate>[1], values?: Record<string, string | number>) =>
      translate('en', key, values),
  }),
}));

const sea: SeaDetails = {
  pol: { id: 'port', kind: 'sea_port', name: 'Порт клиента', country_code: 'RU' },
  vessel: 'Судно заказчика',
  voyage: 'Рейс №42',
};

describe('shipment route localization', () => {
  it('translates labels while preserving names and user-entered details', () => {
    const html = renderToStaticMarkup(<SharedRouteDetailsView sea={sea} customers={[]} />);
    expect(html).toContain('POL · port of loading');
    expect(html).toContain('Vessel');
    expect(html).not.toContain('Дата выхода судна');
    expect(html).toContain('Порт клиента');
    expect(html).toContain('Судно заказчика');
    expect(html).toContain('Рейс №42');
  });
  it('uses localized standard modes instead of the server dictionary caption', () => {
    const html = renderToStaticMarkup(
      <ShipmentRoute
        segments={[{ id: 'leg', type: 'sea', sea }]}
        allowedTypes={[]}
        types={[{ code: 'sea', name: 'Море' }]}
        session={{} as Session}
        customers={[]}
      />,
    );
    expect(html).toContain('Shipment route');
    expect(html).toContain('Sea');
    expect(html).not.toContain('Море');
    expect(html).toContain('Порт клиента');
  });
  it('localizes empty endpoints without changing supplied location names', () => {
    expect(sharedRouteDirection({}, 'en')).toBe('Not specified → Not specified');
    expect(sharedRouteDirection({ sea }, 'en')).toBe('Порт клиента → Not specified');
    expect(sharedRouteDirection({}, 'ru')).toBe('Не указан → Не указан');
  });
});
