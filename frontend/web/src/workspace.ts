export type WorkspaceTab = { id: string; route: string; title: string };
export function workspaceId(route: string) {
  return route.replace(/\/(edit|route)$/, '');
}
export function workspaceTitle(route: string) {
  if (/^\/shipments\/new(?:\/|$)/.test(route)) return 'Новая поставка';
  if (route === '/shipments') return 'Поставки';
  if (route === '/transportations') return 'Перевозки';
  if (route === '/counterparties') return 'Контрагенты';
  if (route === '/counterparties/new') return 'Новый контрагент';
  if (route.startsWith('/shipments/')) return 'Поставка';
  if (route.startsWith('/transportations/')) return 'Перевозка';
  return 'Контрагент';
}
export function openWorkspace(tabs: WorkspaceTab[], route: string): WorkspaceTab[] {
  const id = workspaceId(route);
  const existing = tabs.find((tab) => tab.id === id);
  if (existing) return tabs.map((tab) => (tab.id === id ? { ...tab, route } : tab));
  return [...tabs, { id, route, title: workspaceTitle(route) }];
}
