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
    expect(store.error).toBe('preferences.conflict');
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

it('does not send localized or malformed preference keys to the server', async () => {
  vi.useFakeTimers();
  const { store, transport } = setup();
  store.set('broker:registry:author:guide-contract-view:Основное:workflow:v1', {});
  store.set(`broker:registry:author:${'a'.repeat(201)}`, {});
  await vi.advanceTimersByTimeAsync(400);
  expect(transport.save).not.toHaveBeenCalled();
  expect(store.error).toBe('');
  store.dispose();
});

describe('подписка на конкретную настройку', () => {
  it('изменение и сохранение одного ключа не инвалидирует остальные', async () => {
    vi.useFakeTimers();
    const { store } = setup([entry(['old'])]);
    const other = 'broker:registry:author:contracts:columns:v1';
    const before = store.snapshotFor(other);
    store.set(key, ['new']);
    expect(store.snapshotFor(key)).toBeGreaterThan(0);
    expect(store.snapshotFor(other)).toBe(before);
    await vi.runAllTimersAsync();
    expect(store.snapshotFor(other)).toBe(before);
    store.dispose();
  });
  it('повторная загрузка инвалидирует ключи, включая отсутствующие ранее', async () => {
    const { store } = setup();
    const before = store.snapshotFor(key);
    await store.retry();
    expect(store.snapshotFor(key)).toBeGreaterThan(before);
    store.dispose();
  });
});

it('подтверждение сохранения не меняет snapshot значения и не перезагружает реестр', async () => {
  vi.useFakeTimers();
  const { store } = setup([entry(['old'])]);
  store.set(key, ['new']);
  const changed = store.snapshotFor(key);
  await vi.runAllTimersAsync();
  expect(store.snapshotFor(key)).toBe(changed);
  store.set(key, ['new']);
  expect(store.snapshotFor(key)).toBe(changed);
  store.dispose();
});

it('uses role defaults only when no personal value exists and never imports defaults', () => {
  const transport: PreferenceTransport = {
    list: vi.fn(async () => []),
    save: vi.fn(async (_key, value, revision) => entry(value.value, revision + 1)),
  };
  const store = new PreferenceStore('actor', 'author', 'org', transport, [], storage(), true, {
    'shipments:columns:v1': ['role'],
  });
  expect(store.read(key)).toEqual(['role']);
  store.import(key);
  expect(transport.save).not.toHaveBeenCalled();
  expect(store.readPersonal(key)).toBeUndefined();
  store.set(key, ['personal']);
  expect(store.read(key)).toEqual(['personal']);
  store.dispose();
  const personal = new PreferenceStore(
    'actor',
    'author',
    'org',
    transport,
    [entry(['personal'])],
    storage(),
    true,
    { 'shipments:columns:v1': ['new-role'] },
  );
  expect(personal.read(key)).toEqual(['personal']);
  personal.dispose();
});

it('persists inheritance and does not restore a personal value after reload', async () => {
  vi.useFakeTimers();
  const transport: PreferenceTransport = {
    list: vi.fn(async () => []),
    save: vi.fn(async (_key, value, revision) => entry(value.value, revision + 1)),
  };
  const cache = storage();
  const store = new PreferenceStore(
    'actor',
    'author',
    'org',
    transport,
    [entry(['personal'])],
    cache,
    true,
    {
      'shipments:columns:v1': ['role'],
    },
  );
  expect(store.source(key)).toBe('personal');
  store.reset(key);
  expect(store.read(key)).toEqual(['role']);
  expect(store.source(key)).toBe('role');
  await vi.advanceTimersByTimeAsync(400);
  expect(transport.save).toHaveBeenCalledWith('shipments:columns:v1', { value: null }, 1);
  store.dispose();
  const reloaded = new PreferenceStore(
    'actor',
    'author',
    'org',
    transport,
    [entry(null, 2)],
    cache,
    true,
    {
      'shipments:columns:v1': ['updated role'],
    },
  );
  expect(reloaded.read(key)).toEqual(['updated role']);
  expect(reloaded.source(key)).toBe('role');
  reloaded.dispose();
  vi.useRealTimers();
});
