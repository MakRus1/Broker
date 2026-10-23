import { describe, expect, it } from 'vitest';
import { openWorkspace, replaceWorkspace, workspaceId } from './workspace';
describe('Рабочие вкладки', () => {
  it('сохраняет реестр при открытии карточки', () => {
    const tabs = openWorkspace(openWorkspace([], '/shipments'), '/shipments/abc');
    expect(tabs.map((tab) => tab.route)).toEqual(['/shipments', '/shipments/abc']);
  });
  it('переключает режим редактирования внутри той же вкладки', () => {
    const tabs = openWorkspace(
      openWorkspace([], '/counterparties/abc'),
      '/counterparties/abc/edit',
    );
    expect(tabs).toHaveLength(1);
    expect(tabs[0].route).toBe('/counterparties/abc/edit');
  });
  it('различает несколько новых поставок', () => {
    const tabs = openWorkspace(openWorkspace([], '/shipments/new/a'), '/shipments/new/b');
    expect(tabs).toHaveLength(2);
    expect(workspaceId('/transportations/abc/route')).toBe('/transportations/abc');
  });
});

it('заменяет форму создания сохранённой карточкой без новой вкладки', () => {
  const tabs = openWorkspace(openWorkspace([], '/counterparties'), '/counterparties/new');
  const saved = replaceWorkspace(tabs, '/counterparties/new', '/counterparties/abc', 'Клиент');
  expect(saved).toEqual([
    { id: '/counterparties', route: '/counterparties', title: 'Контрагенты' },
    { id: '/counterparties/abc', route: '/counterparties/abc', title: 'Клиент' },
  ]);
  expect(openWorkspace(saved, '/counterparties/abc')).toHaveLength(2);
});

it('не дублирует уже открытую карточку при сохранении', () => {
  const tabs = openWorkspace(openWorkspace([], '/counterparties/abc'), '/counterparties/new');
  expect(replaceWorkspace(tabs, '/counterparties/new', '/counterparties/abc', 'Клиент')).toEqual([
    { id: '/counterparties/abc', route: '/counterparties/abc', title: 'Клиент' },
  ]);
});
