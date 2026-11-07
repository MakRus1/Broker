import { afterEach, expect, it, vi } from 'vitest';
import { PreferenceStore } from '../../../features/preferences/preferenceStore';
import { claimGuideNotice } from './guideNotice';

const stores: PreferenceStore[] = [];
function store(subject = 'author', entries: ConstructorParameters<typeof PreferenceStore>[4] = []) {
  vi.useFakeTimers();
  const result = new PreferenceStore(
    'actor',
    subject,
    'org',
    {
      list: async () => [],
      save: vi.fn(),
    },
    entries,
  );
  stores.push(result);
  return result;
}
afterEach(() => {
  stores.splice(0).forEach((store) => store.dispose());
  vi.useRealTimers();
});
it('consumes the notice immediately, even when leaving before its timeout', () => {
  const preferences = store();
  expect(claimGuideNotice(preferences, 'author', false)).toBe(true);
  // Другой экземпляр карточки/вкладки использует то же личное хранилище.
  expect(claimGuideNotice(preferences, 'author', false)).toBe(false);
  expect(claimGuideNotice(preferences, 'author', true)).toBe(true);
  expect(claimGuideNotice(preferences, 'author', true)).toBe(false);
});
it('keeps the server-saved decision after reopening and separates users', () => {
  const preferences = store('author', [
    {
      key: 'guide-help:seen-modes:v1',
      value: { value: ['view', 'edit'] },
      revision: 1,
    },
  ]);
  expect(claimGuideNotice(preferences, 'author', false)).toBe(false);
  expect(claimGuideNotice(preferences, 'author', true)).toBe(false);
  expect(claimGuideNotice(store('other'), 'other', false)).toBe(true);
});
it('respects previously shown notices on any record instead of introducing them again', () => {
  const preferences = store('author', [
    {
      key: 'guide-contract:seen-contexts-v3:v1',
      value: { value: ['view-tab-0', 'edit-main'] },
      revision: 1,
    },
  ]);
  expect(claimGuideNotice(preferences, 'author', false)).toBe(false);
  expect(claimGuideNotice(preferences, 'author', true)).toBe(false);
});
it('does not treat a view notice as an introduction to filling fields', () => {
  const preferences = store('author', [
    {
      key: 'guide-shipment:seen-contexts-v3:v1',
      value: { value: ['view:details', null, 12] },
      revision: 1,
    },
  ]);
  expect(claimGuideNotice(preferences, 'author', false)).toBe(false);
  expect(claimGuideNotice(preferences, 'author', true)).toBe(true);
});
it('preserves the shown decision in the local cache before a server save completes', () => {
  const values = new Map<string, string>();
  const cache = {
    getItem: (key: string) => values.get(key) ?? null,
    setItem: (key: string, value: string) => {
      values.set(key, value);
    },
    removeItem: (key: string) => {
      values.delete(key);
    },
    clear: () => values.clear(),
    key: (index: number) => [...values.keys()][index] ?? null,
    get length() {
      return values.size;
    },
  } satisfies Storage;
  const transport = { list: async () => [], save: vi.fn() };
  vi.useFakeTimers();
  const first = new PreferenceStore('actor', 'author', 'org', transport, [], cache);
  stores.push(first);
  expect(claimGuideNotice(first, 'author', false)).toBe(true);
  const reopened = new PreferenceStore('actor', 'author', 'org', transport, [], cache);
  stores.push(reopened);
  expect(claimGuideNotice(reopened, 'author', false)).toBe(false);
  expect(transport.save).not.toHaveBeenCalled();
});
