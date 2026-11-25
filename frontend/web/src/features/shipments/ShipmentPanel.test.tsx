import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import { ShipmentPanel } from './ShipmentPanel';
import type { Shipment } from './types';
import type { Session } from '../../shared/api/session';

const session = {
  actor: 'author',
  subject: 'author',
  organization_id: 'org',
  permissions: ['shipments.write'],
} as Session;
const shipment: Shipment = { id: 'shipment', revision: 1, number: 'S-1', customer_id: 'client' };
const noop = () => {};

describe('container plan in the shipment card', () => {
  function render(readOnly: boolean) {
    return renderToStaticMarkup(
      <ShipmentPanel
        embedded
        readOnly={readOnly}
        containerTypes={['40HC']}
        containerCatalogFailure=""
        session={session}
        initial={shipment}
        customerName="Тестовый клиент"
        onClose={noop}
        onError={noop}
        onUpdate={noop}
      />,
    );
  }
  it('prevents a separate plan mutation while the card is being edited', () => {
    const html = render(true);
    expect(html).toContain('Плановые контейнеры');
    expect(html).toContain('Сохраните изменения карточки');
    expect(html).not.toContain('Добавить позицию');
  });
  it('restores plan actions outside card editing', () => {
    expect(render(false)).toContain('Добавить позицию');
    expect(render(false)).not.toContain('Сохраните изменения карточки');
  });
});
