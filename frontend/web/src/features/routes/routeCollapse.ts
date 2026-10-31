import { useState } from 'react';

export function useRouteCollapse(ids: string[]) {
  const key = `broker:route-expanded:v2:${[...ids].sort().join(':')}`;
  const [expanded, setExpanded] = useState<Set<string>>(() => {
    try {
      const saved: unknown = JSON.parse(sessionStorage.getItem(key) ?? '[]');
      return new Set(
        Array.isArray(saved)
          ? saved.filter((id) => typeof id === 'string' && ids.includes(id))
          : [],
      );
    } catch {
      return new Set();
    }
  });
  function save(next: Set<string>) {
    setExpanded(next);
    try {
      sessionStorage.setItem(key, JSON.stringify([...next]));
    } catch {
      /* Сворачивание работает без хранилища. */
    }
  }
  return {
    collapsed: new Set(ids.filter((id) => !expanded.has(id))),
    toggle: (id: string, closed: boolean) => {
      if (expanded.has(id) === !closed) return;
      const next = new Set(expanded);
      if (closed) next.delete(id);
      else next.add(id);
      save(next);
    },
    collapseAll: () => save(new Set<string>()),
    expandAll: () => save(new Set(ids)),
  };
}
