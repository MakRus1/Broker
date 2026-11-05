import { beforeEach, afterEach, expect, it, vi } from 'vitest';
import { useActivationLoad } from './useActivationLoad';

// Exercise effect lifecycles, including aborts, without a browser renderer.
const hooks = vi.hoisted(() => ({
  refs: [] as { current: unknown }[],
  index: 0,
  deps: undefined as unknown[] | undefined,
  cleanup: undefined as (() => void) | undefined,
}));
vi.mock('react', () => ({
  useRef: (initial: unknown) => {
    const index = hooks.index++;
    return (hooks.refs[index] ??= { current: initial });
  },
  useEffect: (effect: () => (() => void) | undefined, deps: unknown[]) => {
    if (hooks.deps && deps.every((value, index) => Object.is(value, hooks.deps![index]))) return;
    hooks.cleanup?.();
    hooks.deps = deps;
    hooks.cleanup = effect();
  },
}));
function render(active: boolean, load: (signal: AbortSignal) => Promise<void>, error = vi.fn()) {
  hooks.index = 0;
  useActivationLoad(active, load, error);
}
beforeEach(() => {
  hooks.refs = [];
  hooks.deps = undefined;
  hooks.cleanup = undefined;
  vi.useFakeTimers();
});
afterEach(() => {
  hooks.cleanup?.();
  vi.useRealTimers();
});
it('does not load hidden sections and reuses a completed load on quick return', async () => {
  const load = vi.fn(async () => {});
  render(false, load);
  expect(load).not.toHaveBeenCalled();
  render(true, load);
  await Promise.resolve();
  render(false, load);
  render(true, load);
  expect(load).toHaveBeenCalledTimes(1);
  render(false, load);
  vi.advanceTimersByTime(30_000);
  render(true, load);
  expect(load).toHaveBeenCalledTimes(2);
});
it('reloads when the resource or session changes the loader', async () => {
  const first = vi.fn(async () => {});
  const second = vi.fn(async () => {});
  render(true, first);
  await Promise.resolve();
  render(true, second);
  expect(second).toHaveBeenCalledTimes(1);
});
it('does not restart requests when only the error callback changes', () => {
  const load = vi.fn(async () => {});
  render(true, load, vi.fn());
  render(true, load, vi.fn());
  expect(load).toHaveBeenCalledTimes(1);
});
it('aborts an inactive load and retries even if the cancelled request later resolves', async () => {
  let complete!: () => void;
  const load = vi.fn(
    (signal: AbortSignal) =>
      new Promise<void>((resolve) => {
        complete = resolve;
      }),
  );
  render(true, load);
  render(false, load);
  expect(load.mock.calls[0][0].aborted).toBe(true);
  complete();
  await Promise.resolve();
  render(true, load);
  expect(load).toHaveBeenCalledTimes(2);
});
it('reports failures and retries them on the next activation', async () => {
  const error = new Error('Unavailable');
  const load = vi.fn(async () => {
    throw error;
  });
  const onError = vi.fn();
  render(true, load, onError);
  await Promise.resolve();
  await Promise.resolve();
  expect(onError).toHaveBeenCalledWith(error);
  render(false, load, onError);
  render(true, load, onError);
  expect(load).toHaveBeenCalledTimes(2);
});
