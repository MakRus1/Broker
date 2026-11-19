import { createElement } from 'react';
import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it } from 'vitest';
import { RoutePlanView } from './RoutePlan';
import { sharedRouteDirection } from './SharedRouteDetails';
import { routeDirection } from '../transportations/transportation';

const plan = {
  origin_name: 'Роттердам',
  origin_country: 'NL',
  origin_address: 'Терминал',
  destination_name: 'Москва',
  destination_country: 'RU',
  destination_address: 'Склад',
  departure_date: '2030-01-10',
  arrival_date: '2030-01-15',
};
it('shows both endpoints and calendar dates in inquiry and shipment plans', () => {
  const html = renderToStaticMarkup(createElement(RoutePlanView, { value: plan }));
  for (const value of ['Роттердам', 'Москва', 'Терминал', 'Склад', '10.01.2030', '15.01.2030'])
    expect(html).toContain(value);
  expect(sharedRouteDirection({ plan }, 'ru')).toBe('Роттердам → Москва');
});
it('uses the source plan as a fallback without hiding execution locations', () => {
  expect(routeDirection({ id: 'leg', mode: 'international_road', plan })).toBe(
    'Роттердам → Москва',
  );
  expect(
    routeDirection({ id: 'leg', mode: 'pickup', plan, pickup: { delivery_city: 'Казань' } }),
  ).toBe('Казань');
});
