// Общая ревизия данных: успешная запись делает загруженные реестры устаревшими.
// Настройки пользователя, переводы и запросы чтения её не меняют.
let revision = 0;
let version = 0;
const listeners = new Set<() => void>();
export function isDataMutation(path: string): boolean {
  return /^\/api\/v1\/.+\/(save|create|update|set|set-archived|add|remove|delete|add-service-charge|detach-service-charge)$/.test(
    path,
  );
}
export function invalidateRegistryData() {
  revision++;
  notify();
}
function notify() {
  version++;
  listeners.forEach((listener) => listener());
}
export const registryDataRevision = () => revision;
export const registryActivitySnapshot = () => version;
export function subscribeRegistryActivity(listener: () => void) {
  if (!listeners.size && typeof window !== 'undefined') {
    window.addEventListener('focus', notify);
    document.addEventListener('visibilitychange', onVisible);
  }
  listeners.add(listener);
  return () => {
    listeners.delete(listener);
    if (!listeners.size && typeof window !== 'undefined') {
      window.removeEventListener('focus', notify);
      document.removeEventListener('visibilitychange', onVisible);
    }
  };
}
function onVisible() {
  if (document.visibilityState === 'visible') notify();
}
