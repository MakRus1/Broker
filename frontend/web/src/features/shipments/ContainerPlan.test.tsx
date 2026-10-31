import { describe, expect, it } from 'vitest';
import { renderToStaticMarkup } from 'react-dom/server';
import { PlanQuantityForm } from './ContainerPlan';
import type { Item } from './types';

const item: Item = {
  id: 'plan',
  revision: 1,
  shipment_id: 'shipment',
  container_type_code: '40HC',
  container_ownership: 'COC',
  planned_quantity: 2,
  created_quantity: 4,
  pending_quantity: 0,
  available_quantity: 0,
  excess_quantity: 2,
};
const save = async () => {};
const fail = () => {};
describe('изменение плановых контейнеров', () => {
  it('предупреждает о превышении, но оставляет сохранение доступным', () => {
    const html = renderToStaticMarkup(
      <PlanQuantityForm item={item} onSubmit={save} onError={fail} />,
    );
    expect(html).toContain('Превышение плана на 2');
    expect(html).toContain('Сохранение разрешено');
    expect(html).not.toContain('disabled');
  });
  it('разрешает нулевой план и сохраняет выбранную принадлежность', () => {
    const html = renderToStaticMarkup(
      <PlanQuantityForm item={{ ...item, planned_quantity: 0 }} onSubmit={save} onError={fail} />,
    );
    expect(html).toContain('min="0"');
    expect(html).toContain('value="0"');
    expect(html).toContain('40HC');
    expect(html).toContain('COC');
  });
});
