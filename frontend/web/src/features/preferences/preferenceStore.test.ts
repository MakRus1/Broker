import { afterEach, describe, expect, it, vi } from 'vitest';
import { PreferenceStore, type Preference, type PreferenceTransport } from './preferenceStore';

const key = 'broker:registry:author:shipments:columns:v1';
const entry = (value: unknown, revision = 1): Preference => ({
  key: 'shipments:columns:v1',
  value: { value },
  revision,
});
function storage(): Storage {
  const data = new Map<string, string>();
  return {
    getItem: (key) => data.get(key) ?? null,
    setItem: (key, value) => {
      data.set(key, value);
    },
    removeItem: (key) => {
      data.delete(key);
    },
    clear: () => data.clear(),
    key: (i) => [...data.keys()][i] ?? null,
    get length() {
      return data.size;
    },
  };
}
function setup(
  entries: Preference[] = [],
  cache = storage(),
  organization = 'org-a',
  ready = true,
) {
  const transport: PreferenceTransport = {
    list: vi.fn(async () => entries),
    save: vi.fn(async (_key, value, revision) => entry(value.value, revision + 1)),
  };
  const store = new PreferenceStore(
    'issuer#author',
    'author',
    organization,
    transport,
    entries,
    cache,
    ready,
  );
  return { store, transport, cache };
}
afterEach(() => vi.useRealTimers());
describe('серверные личные настройки', () => {
  it('сервер имеет приоритет, значения по умолчанию не записываются', async () => {
    vi.useFakeTimers();
    const cache = storage();
    cache.setItem(key, JSON.stringify(['legacy']));
    const { store, transport } = setup([entry(['server'])], cache);
    expect(store.read(key)).toEqual(['server']);
    store.import(key);
    await vi.runAllTimersAsync();
    expect(transport.save).not.toHaveBeenCalled();
  });
  it('объединяет частые изменения и передаёт текущую ревизию', async () => {
    vi.useFakeTimers();
    const { store, transport } = setup([entry(['old'], 4)]);
    store.set(key, ['first']);
    store.set(key, ['last']);
    await vi.runAllTimersAsync();
    expect(transport.save).toHaveBeenCalledExactlyOnceWith(
      'shipments:columns:v1',
      { value: ['last'] },
      4,
    );
  });
  it('сохраняет новое изменение после завершения предыдущего запроса', async () => {
    vi.useFakeTimers();
    const { store, transport } = setup();
    let finish!: (value: Preference) => void;
    vi.mocked(transport.save).mockImplementationOnce(
      () =>
        new Promise((resolve) => {
          finish = resolve;
        }),
    );
    store.set(key, ['first']);
    await vi.advanceTimersByTimeAsync(350);
    store.set(key, ['second']);
    finish(entry(['first']));
    await vi.runAllTimersAsync();
    expect(transport.save).toHaveBeenLastCalledWith(
      'shipments:columns:v1',
      { value: ['second'] },
      1,
    );
    expect(store.read(key)).toEqual(['second']);
  });
  it('импортирует старое хранилище только в одну организацию', async () => {
    vi.useFakeTimers();
    const cache = storage();
    cache.setItem(key, JSON.stringify(['legacy']));
    const { store, transport } = setup([], cache);
    store.import(key);
    await vi.runAllTimersAsync();
    expect(transport.save).toHaveBeenCalledOnce();
    expect(setup([], cache, 'org-b').store.read(key)).toBeUndefined();
    expect(setup([], cache, 'org-a').store.read(key)).toEqual(['legacy']);
  });
  it('не перезаписывает чужую ревизию при конфликте', async () => {
    vi.useFakeTimers();
    const { store, transport } = setup([entry(['old'])]);
    vi.mocked(transport.save).mockRejectedValue({ status: 409 });
    vi.mocked(transport.list).mockResolvedValue([entry(['other device'], 2)]);
    store.set(key, ['mine']);
    await vi.runAllTimersAsync();
    expect(store.read(key)).toEqual(['other device']);
    expect(transport.save).toHaveBeenCalledOnce();
    expect(store.error).toContain('другом окне');
  });
  it('не пишет настройки при неудачной загрузке и повторяет после восстановления', async () => {
    vi.useFakeTimers();
    const { store, transport } = setup([], storage(), 'org-a', false);
    store.set(key, ['offline']);
    await vi.runAllTimersAsync();
    expect(transport.save).not.toHaveBeenCalled();
    await store.retry();
    expect(transport.save).toHaveBeenCalledOnce();
    expect(store.error).toBe('');
  });
});
