export const REGISTRY_MAX_AGE_MS = 60_000;

// Данные и курсоры остаются в компоненте. Здесь только актуальность одной загрузки,
// без глобального кеша ответов и накопления предыдущих вариантов фильтров.
export class RegistryFreshness {
  private loaded?: { identity: readonly unknown[]; revision: number; at: number };
  clear() {
    this.loaded = undefined;
  }
  isFresh(identity: readonly unknown[], revision: number, now = Date.now()) {
    const loaded = this.loaded;
    return (
      !!loaded &&
      loaded.revision === revision &&
      loaded.identity.length === identity.length &&
      loaded.identity.every((value, index) => Object.is(value, identity[index])) &&
      now >= loaded.at &&
      now - loaded.at < REGISTRY_MAX_AGE_MS
    );
  }
  complete(identity: readonly unknown[], revision: number, now = Date.now()) {
    this.loaded = { identity: [...identity], revision, at: now };
  }
}
