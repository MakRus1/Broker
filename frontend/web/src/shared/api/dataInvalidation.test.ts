import { afterEach, expect, it, vi } from 'vitest';
import { isDataMutation, registryDataRevision } from './dataInvalidation';
import { request } from './http';
afterEach(() => vi.unstubAllGlobals());
it('инвалидирует после записи, но не после чтения или сохранения настроек', async () => {
  vi.stubGlobal(
    'fetch',
    vi.fn(async () => new Response('{}', { status: 200 })),
  );
  const before = registryDataRevision();
  await request('/api/v1/invoices/list', undefined, {});
  await request('/api/preferences/save', undefined, {});
  expect(registryDataRevision()).toBe(before);
  await request('/api/v1/payments/save', undefined, {});
  expect(registryDataRevision()).toBe(before + 1);
});
it('ошибка записи не инвалидирует загруженные реестры', async () => {
  vi.stubGlobal(
    'fetch',
    vi.fn(async () => new Response('{}', { status: 409 })),
  );
  const before = registryDataRevision();
  await expect(request('/api/v1/invoices/save', undefined, {})).rejects.toThrow();
  expect(registryDataRevision()).toBe(before);
});
it('учитывает изменение маршрута и связей счёта, исключает поисковые ручки', () => {
  for (const path of [
    '/api/v1/transportation-routes/set',
    '/api/v1/counterparties/set-archived',
    '/api/v1/invoices/add-service-charge',
    '/api/v1/invoices/detach-service-charge',
  ])
    expect(isDataMutation(path)).toBe(true);
  for (const path of [
    '/api/v1/invoices/candidates',
    '/api/v1/payments/invoice-balance',
    '/api/v1/shipments/filter-values',
    '/api/localization/save',
  ])
    expect(isDataMutation(path)).toBe(false);
});
