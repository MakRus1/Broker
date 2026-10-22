import { describe, expect, it } from 'vitest';
import { openWorkspace, workspaceId } from './workspace';
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
