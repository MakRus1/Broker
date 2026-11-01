import { describe, expect, it } from 'vitest';
import {
  openWorkspace,
  replaceWorkspace,
  workspaceId,
  moveWorkspace,
  restoreWorkspaces,
} from './workspace';
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

describe('Порядок рабочих вкладок', () => {
  const tabs = ['/shipments', '/transportations', '/counterparties'].reduce(
    openWorkspace,
    [] as ReturnType<typeof openWorkspace>,
  );
  it('переносит вкладку в обе стороны и сохраняет данные вкладок', () => {
    const moved = moveWorkspace(tabs, '/shipments', '/counterparties', 'after');
    expect(moved).toEqual([tabs[1], tabs[2], tabs[0]]);
    expect(moveWorkspace(moved, '/shipments', '/transportations', 'before')).toEqual(tabs);
    expect(tabs.map((tab) => tab.id)).toEqual([
      '/shipments',
      '/transportations',
      '/counterparties',
    ]);
  });
  it('не теряет вкладки при отменённом переносе или устаревшем идентификаторе', () => {
    expect(moveWorkspace(tabs, '/shipments', '/shipments', 'after')).toBe(tabs);
    expect(moveWorkspace(tabs, '/missing', '/shipments', 'before')).toBe(tabs);
    expect(moveWorkspace(tabs, '/shipments', '/missing', 'after')).toBe(tabs);
  });
  it('восстанавливает порядок и оставляет текущую вкладку на прежнем месте', () => {
    const moved = moveWorkspace(tabs, '/shipments', '/counterparties', 'after');
    expect(restoreWorkspaces(JSON.stringify(moved), '/transportations')).toEqual(moved);
  });
  it('отбрасывает повреждённые и повторные записи, открывает текущий адрес', () => {
    expect(restoreWorkspaces('broken', '/shipments')).toEqual([tabs[0]]);
    expect(
      restoreWorkspaces(
        JSON.stringify([tabs[1], tabs[1], { route: 'https://example.com', title: 'bad' }, null]),
        '/shipments',
      ),
    ).toEqual([tabs[1], tabs[0]]);
  });
});

it('открывает каждую услугу в отдельной вкладке и повторно использует её', () => {
  const route = '/service-charges/11111111-1111-4111-8111-111111111111';
  const tabs = openWorkspace(openWorkspace([], '/service-charges'), route);
  expect(tabs.map((tab) => tab.title)).toEqual(['Услуги', 'Услуга']);
  expect(openWorkspace(tabs, route)).toHaveLength(2);
  expect(restoreWorkspaces(JSON.stringify(tabs), route)).toEqual(tabs);
});
