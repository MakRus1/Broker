import { describe, expect, it, vi } from 'vitest';
import { RegistryLoader, type RegistryPage } from './registryLoader';
const cursor = { id: 'id', value: 'value', scope: 'scope' };
function pending<T>() {
  let resolve!: (value: T) => void;
  let reject!: (reason: Error) => void;
  const promise = new Promise<T>((yes, no) => {
    resolve = yes;
    reject = no;
  });
  return { promise, resolve, reject };
}
describe('загрузка административных реестров', () => {
  it('отменяет старый фильтр и не принимает его запоздавший ответ', async () => {
    const first = pending<RegistryPage<string>>();
    const second = pending<RegistryPage<string>>();
    const fetch = vi.fn().mockReturnValueOnce(first.promise).mockReturnValueOnce(second.promise);
    const loader = new RegistryLoader<string>(fetch, () => {});
    const old = loader.start({ query: 'старый' });
    const next = loader.start({ query: 'новый' });
    expect(fetch.mock.calls[0][1].aborted).toBe(true);
    first.resolve({ items: ['устаревший'] });
    await old;
    expect(loader.state.items).toEqual([]);
    expect(loader.state.loading).toBe(true);
    second.resolve({ items: ['актуальный'] });
    await next;
    expect(loader.state.items).toEqual(['актуальный']);
  });
  it('загружает курсор один раз при повторных событиях наблюдателя', async () => {
    const next = pending<RegistryPage<string>>();
    const fetch = vi
      .fn()
      .mockResolvedValueOnce({ items: ['первая'], next_cursor: cursor })
      .mockReturnValueOnce(next.promise);
    const loader = new RegistryLoader<string>(fetch, () => {});
    await loader.start({ sort: 'name' });
    const loading = loader.more();
    await loader.more();
    expect(fetch).toHaveBeenCalledTimes(2);
    expect(fetch.mock.calls[1][0]).toEqual({ sort: 'name', limit: 50, cursor });
    next.resolve({ items: ['вторая'] });
    await loading;
    await loader.more();
    expect(fetch).toHaveBeenCalledTimes(2);
    expect(loader.state.items).toEqual(['первая', 'вторая']);
  });
  it('при ошибке продолжения сохраняет строки и повторяет тот же курсор', async () => {
    const fetch = vi
      .fn()
      .mockResolvedValueOnce({ items: ['первая'], next_cursor: cursor })
      .mockRejectedValueOnce(new Error('Временно недоступно'))
      .mockResolvedValueOnce({ items: ['вторая'] });
    const loader = new RegistryLoader<string>(fetch, () => {});
    await loader.start({});
    await loader.more();
    expect(loader.state.items).toEqual(['первая']);
    expect(loader.state.error).toBe('Временно недоступно');
    await loader.more();
    expect(fetch.mock.calls[2][0]).toEqual(fetch.mock.calls[1][0]);
    expect(loader.state.items).toEqual(['первая', 'вторая']);
    expect(loader.state.error).toBe('');
  });
  it('повторяет первую страницу после ошибки и игнорирует ответ после остановки', async () => {
    const last = pending<RegistryPage<string>>();
    const fetch = vi
      .fn()
      .mockRejectedValueOnce(new Error('Сбой'))
      .mockReturnValueOnce(last.promise);
    const changed = vi.fn();
    const loader = new RegistryLoader<string>(fetch, changed);
    await loader.start({ enabled: false });
    const retry = loader.more();
    loader.stop();
    const calls = changed.mock.calls.length;
    last.resolve({ items: ['после ухода'] });
    await retry;
    expect(changed).toHaveBeenCalledTimes(calls);
    expect(fetch.mock.calls[1][0]).toEqual({ enabled: false, limit: 50 });
  });
});
