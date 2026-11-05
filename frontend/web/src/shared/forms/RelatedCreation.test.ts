import { afterEach, beforeEach, expect, it, vi } from 'vitest';
import { CreationRequests, relatedCreationRoute } from './RelatedCreation';
import { isWorkspaceRoute, workspaceTitle } from '../../app/workspace';

const source = '/outgoing-invoices/new';
beforeEach(() => {
  vi.stubGlobal('location', { hash: '#' + source });
  vi.stubGlobal('document', { title: 'Новый счёт · Broker', querySelectorAll: () => [] });
});
afterEach(() => vi.unstubAllGlobals());
it('creates independent full-form workspaces without replacing an existing create tab', () => {
  const requests = new CreationRequests();
  const first = requests.start('counterparty', {}, vi.fn());
  const second = requests.start('counterparty', { ownCompany: true }, vi.fn());
  expect(first).not.toBe(second);
  for (const route of [first, second]) {
    expect(isWorkspaceRoute(route)).toBe(true);
    expect(workspaceTitle(route)).toBe('Новый контрагент');
  }
  expect(requests.pending.get(first)?.origin).toBe(source);
  expect(requests.pending.get(second)?.origin).toBe(first);
});
it('selects a saved record once and returns to the exact source route', () => {
  location.hash = '#/contracts/11111111-1111-4111-8111-111111111111/edit';
  const requests = new CreationRequests();
  const select = vi.fn();
  const route = requests.start('bankAccount', { owner: 'company', currency: 'USD' }, select);
  const result = { id: 'saved-account' };
  expect(requests.finish(route, result)).toBe(
    '/contracts/11111111-1111-4111-8111-111111111111/edit',
  );
  expect(select).toHaveBeenCalledExactlyOnceWith(result);
  expect(requests.finish(route, result)).toBeUndefined();
});
it('cancel returns without changing the originating selection', () => {
  const requests = new CreationRequests();
  const select = vi.fn();
  const route = requests.start(
    'contract',
    { counterparty_id: 'customer', currency: 'USD' },
    select,
  );
  expect(requests.pending.get(route)).toMatchObject({
    kind: 'contract',
    originTitle: 'Новый счёт',
    defaults: { counterparty_id: 'customer', currency: 'USD' },
  });
  expect(requests.cancel(route)).toBe(source);
  expect(select).not.toHaveBeenCalled();
  expect(requests.pending.size).toBe(0);
});
it('keeps nested creation callbacks separate', () => {
  const requests = new CreationRequests();
  const contractSelected = vi.fn();
  const accountSelected = vi.fn();
  const contract = requests.start('contract', {}, contractSelected);
  const account = requests.start(
    'bankAccount',
    { owner: 'company', currency: 'RUB' },
    accountSelected,
  );
  expect(requests.finish(account, { id: 'account' })).toBe(contract);
  expect(contractSelected).not.toHaveBeenCalled();
  expect(requests.finish(contract, { id: 'contract' })).toBe(source);
  expect(accountSelected).toHaveBeenCalledTimes(1);
  expect(contractSelected).toHaveBeenCalledTimes(1);
});
it('does not deliver results after a source field or session is gone', () => {
  const requests = new CreationRequests();
  const select = vi.fn();
  const route = requests.start('counterparty', {}, select);
  requests.pending.delete(route);
  expect(requests.finish(route, {})).toBeUndefined();
  requests.start('contract', {}, select);
  requests.clear();
  expect(requests.pending.size).toBe(0);
  expect(select).not.toHaveBeenCalled();
});
it('suspends an originating dialog and restores it only on return', () => {
  const dialog = {
    open: true,
    isConnected: true,
    close: vi.fn(function (this: { open: boolean }) {
      this.open = false;
    }),
    showModal: vi.fn(function (this: { open: boolean }) {
      this.open = true;
    }),
  };
  vi.stubGlobal('document', { title: 'Новый счёт · Broker', querySelectorAll: () => [dialog] });
  const requests = new CreationRequests();
  const route = requests.start('bankAccount', { owner: 'company', currency: 'USD' }, vi.fn());
  expect(dialog.open).toBe(false);
  const origin = requests.cancel(route)!;
  requests.restoreDialogs('/shipments');
  expect(dialog.showModal).not.toHaveBeenCalled();
  requests.restoreDialogs(origin);
  expect(dialog.showModal).toHaveBeenCalledTimes(1);
});
it('supports all related full-form routes', () => {
  const id = '11111111-1111-4111-8111-111111111111';
  for (const kind of ['counterparty', 'contract', 'bankAccount', 'financialLabel'] as const)
    expect(isWorkspaceRoute(relatedCreationRoute(kind, id))).toBe(true);
});

it('opens incoming and outgoing invoices with the source context and returns the selection', () => {
  for (const direction of ['incoming', 'outgoing'] as const) {
    location.hash = '#/service-charges/service';
    const requests = new CreationRequests();
    const select = vi.fn();
    const defaults = {
      direction,
      counterparty_id: 'party',
      counterparty_name: 'Party',
      contract_id: null,
      contract_number: null,
      currency: 'USD',
    };
    const route = requests.start('invoice', defaults, select);
    expect(route).toMatch(new RegExp(`^/${direction}-invoices/new/`));
    expect(isWorkspaceRoute(route)).toBe(true);
    expect(requests.pending.get(route)?.defaults).toEqual(defaults);
    expect(requests.finish(route, { id: 'invoice' })).toBe('/service-charges/service');
    expect(select).toHaveBeenCalledExactlyOnceWith({ id: 'invoice' });
  }
});
