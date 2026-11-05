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
