export type WorkspaceTab = { id: string; route: string; title: string };
export function workspaceId(route: string) {
  return route.replace(/\/(edit|route)$/, '');
}
export function workspaceTitle(route: string) {
  if (/^\/shipments\/new(?:\/|$)/.test(route)) return 'Новая поставка';
  if (route === '/shipments') return 'Поставки';
  if (route.startsWith('/transportations/new/')) return 'Новая перевозка';
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

export function replaceWorkspace(
  tabs: WorkspaceTab[],
  previous: string,
  route: string,
  title: string,
): WorkspaceTab[] {
  const id = workspaceId(route);
  return tabs
    .filter((tab) => tab.id === previous || tab.id !== id)
    .map((tab) => (tab.id === previous ? { id, route, title } : tab));
}

export function moveWorkspace(
  tabs: WorkspaceTab[],
  source: string,
  target: string,
  side: 'before' | 'after',
): WorkspaceTab[] {
  if (source === target) return tabs;
  const moving = tabs.find((tab) => tab.id === source);
  if (!moving || !tabs.some((tab) => tab.id === target)) return tabs;
  const result = tabs.filter((tab) => tab.id !== source);
  const index = result.findIndex((tab) => tab.id === target);
  result.splice(index + (side === 'after' ? 1 : 0), 0, moving);
  return result;
}

export function restoreWorkspaces(raw: string | null, route: string): WorkspaceTab[] {
  try {
    const saved: unknown = JSON.parse(raw ?? 'null');
    if (!Array.isArray(saved)) return openWorkspace([], route);
    const tabs: WorkspaceTab[] = [];
    for (const value of saved) {
      if (
        !value ||
        typeof value.route !== 'string' ||
        typeof value.title !== 'string' ||
        !/^\/(shipments|transportations|counterparties)(?:\/(?:new(?:\/[0-9a-f-]{36})?|[0-9a-f-]{36})(?:\/(?:route|edit))?)?$/i.test(
          value.route,
        )
      )
        continue;
      const id = workspaceId(value.route);
      if (!tabs.some((tab) => tab.id === id))
        tabs.push({ id, route: value.route, title: value.title });
    }
    return openWorkspace(tabs, route);
  } catch {
    return openWorkspace([], route);
  }
}
