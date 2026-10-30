import { expect, it } from 'vitest';
import { invoiceRegistry, invoiceTitle } from './invoices';
import { openWorkspace, restoreWorkspaces, workspaceTitle } from './workspace';
it('разделяет входящие и исходящие счета и восстанавливает их вкладки', () => {
  expect(invoiceRegistry('incoming')).toBe('/incoming-invoices');
  expect(invoiceTitle('outgoing')).toBe('Исходящие счета');
  const route = '/incoming-invoices/11111111-1111-4111-8111-111111111111';
  const tabs = openWorkspace([], route);
  expect(restoreWorkspaces(JSON.stringify(tabs), route)).toEqual(tabs);
  expect(workspaceTitle('/outgoing-invoices/new')).toBe('Новый счёт');
  const seeded = '/outgoing-invoices/new/11111111-1111-4111-8111-111111111111';
  expect(workspaceTitle(seeded)).toBe('Новый счёт');
  expect(restoreWorkspaces(JSON.stringify(openWorkspace([], seeded)), seeded)).toHaveLength(1);
});
