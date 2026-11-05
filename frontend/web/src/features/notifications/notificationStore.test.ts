import { expect, it } from 'vitest';
import { NotificationStore } from './notificationStore';
it('deduplicates a persistent failure and reopens it after recovery', () => {
  const store = new NotificationStore();
  const input = {
    id: 'preferences',
    level: 'warning' as const,
    message: 'Cannot save',
    action: { label: 'Retry', run() {} },
  };
  store.publish(input);
  const first = store.snapshot();
  store.publish(input);
  expect(store.snapshot()).toBe(first);
  store.resolve('preferences');
  expect(store.snapshot()[0]).toMatchObject({
    resolved: true,
    read: true,
    toast: false,
    action: undefined,
  });
  store.publish(input);
  expect(store.snapshot()).toHaveLength(1);
  expect(store.snapshot()[0]).toMatchObject({ resolved: false, read: false, toast: true });
});
it('hiding a toast keeps its unread history; reading and clearing are independent', () => {
  const store = new NotificationStore();
  store.publish({ id: 'one', level: 'success', message: 'Done' });
  store.publish({ id: 'two', level: 'error', message: 'Failed' });
  store.hideToast('one');
  expect(store.snapshot().find((item) => item.id === 'one')).toMatchObject({
    read: false,
    toast: false,
  });
  store.markRead('one');
  store.clearRead();
  expect(store.snapshot().map((item) => item.id)).toEqual(['two']);
  store.markRead();
  store.clearRead();
  expect(store.snapshot()).toEqual([]);
});
it('bounds history and keeps different sessions isolated', () => {
  const first = new NotificationStore();
  const other = new NotificationStore();
  for (let i = 0; i < 120; i++)
    first.publish({ id: String(i), level: 'success', message: String(i) });
  expect(first.snapshot()).toHaveLength(100);
  expect(first.snapshot()[0].id).toBe('119');
  expect(first.snapshot().at(-1)?.id).toBe('20');
  expect(other.snapshot()).toEqual([]);
});

const remote = (id = 'server:one', read = false) => ({
  id,
  remote: true,
  read,
  createdAt: 100,
  level: 'info' as const,
  message: 'Route changed',
  toast: false,
  resolved: false,
});
it('keeps server history separate from local toasts and merges duplicate polls', () => {
  const store = new NotificationStore();
  store.publish({ id: 'local', message: 'Saved', level: 'success' });
  store.mergeRemote([remote()]);
  store.mergeRemote([remote()]);
  expect(store.snapshot()).toHaveLength(2);
  expect(store.snapshot().find((item) => item.remote)?.toast).toBe(false);
});
it('does not resurrect unread state from a poll started before marking read', async () => {
  const store = new NotificationStore();
  store.setRemoteRead(async () => {});
  store.mergeRemote([remote()]);
  store.markRead('server:one');
  await Promise.resolve();
  store.mergeRemote([remote()]);
  expect(store.snapshot()[0].read).toBe(true);
  store.mergeRemote([remote('server:one', true)]);
  expect(store.snapshot()[0].read).toBe(true);
});
it('rolls back failed read persistence and restores failed dismissal', async () => {
  const store = new NotificationStore();
  store.setRemoteRead(async () => {
    throw new Error('offline');
  });
  store.mergeRemote([remote()]);
  store.markRead('server:one');
  await Promise.resolve();
  await Promise.resolve();
  expect(store.snapshot()[0].read).toBe(false);
  store.mergeRemote([remote('server:one', true)]);
  store.clearRead();
  expect(store.snapshot()).toHaveLength(0);
  await Promise.resolve();
  await Promise.resolve();
  expect(store.snapshot()).toHaveLength(1);
});
it('dismissed server items stay hidden on subsequent polls', async () => {
  const store = new NotificationStore();
  const calls: unknown[] = [];
  store.setRemoteRead(async (ids, dismiss) => {
    calls.push({ ids, dismiss });
  });
  store.mergeRemote([remote('server:one', true)]);
  store.clearRead();
  store.mergeRemote([remote('server:one', true)]);
  expect(store.snapshot()).toHaveLength(0);
  expect(calls).toEqual([{ ids: ['server:one'], dismiss: true }]);
});

it('announces only new unread server events after the initial history snapshot', () => {
  const store = new NotificationStore();
  store.mergeRemote([remote('server:history')]);
  expect(store.snapshot()[0].toast).toBe(false);
  store.mergeRemote([remote('server:new'), remote('server:history')]);
  expect(store.snapshot().find((item) => item.id === 'server:new')?.toast).toBe(true);
  store.hideToast('server:new');
  store.mergeRemote([remote('server:new'), remote('server:history')]);
  expect(store.snapshot().some((item) => item.toast)).toBe(false);
  store.mergeRemote([{ ...remote('server:new'), message: 'Translated route changed' }]);
  expect(store.snapshot().some((item) => item.toast)).toBe(false);
});
it('does not replay existing history on reload or after regaining access', () => {
  const store = new NotificationStore();
  store.mergeRemote([]);
  store.mergeRemote([remote()]);
  expect(store.snapshot()[0].toast).toBe(true);
  const reloaded = new NotificationStore();
  reloaded.mergeRemote([remote()]);
  expect(reloaded.snapshot()[0].toast).toBe(false);
  store.clearRemote();
  store.mergeRemote([remote()]);
  expect(store.snapshot()[0].toast).toBe(false);
});
it('does not announce read events and bounds burst toasts without a delayed cascade', () => {
  const store = new NotificationStore();
  store.mergeRemote([]);
  const batch = Array.from({ length: 10 }, (_, i) => remote(`server:${i}`));
  store.mergeRemote([remote('server:read', true), ...batch]);
  expect(store.snapshot().filter((item) => item.toast)).toHaveLength(3);
  for (const item of store.snapshot()) store.hideToast(item.id);
  store.mergeRemote(batch);
  expect(store.snapshot().some((item) => item.toast)).toBe(false);
  expect(store.snapshot().filter((item) => !item.read)).toHaveLength(10);
});
