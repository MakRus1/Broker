import { workspaceSections } from './workspaceSections';
export function restoreFavorites(raw: unknown): string[] {
  return Array.isArray(raw)
    ? [
        ...new Set(
          raw.filter(
            (value): value is string =>
              typeof value === 'string' &&
              value.length <= 160 &&
              /^(section|view):[a-z0-9_:/-]+$/i.test(value),
          ),
        ),
      ].slice(0, 60)
    : [];
}
export function favoriteSection(route: string) {
  return `section:${route}`;
}
export function favoriteView(registry: string, id: string) {
  return `view:${registry}:${id}`;
}
export function visibleSections(permissions: string[]) {
  return workspaceSections.filter((section) => permissions.includes(section.permission));
}
