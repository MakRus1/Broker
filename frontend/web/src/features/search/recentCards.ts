import type { SearchHit } from './searchSources';

export function restoreRecentCards(raw: unknown): SearchHit[] {
  if (!Array.isArray(raw)) return [];
  return raw
    .filter(
      (v) =>
        v &&
        typeof v.title === 'string' &&
        v.title.length <= 300 &&
        typeof v.route === 'string' &&
        /^\/[a-z-]+\/[0-9a-f-]{36}$/i.test(v.route) &&
        typeof v.id === 'string',
    )
    .slice(0, 12);
}
