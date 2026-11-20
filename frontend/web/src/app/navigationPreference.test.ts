import { describe, expect, it } from 'vitest';
import { readNavigationMode } from './navigationPreference';

const storage = (values: Record<string, string>) => ({
  getItem: (key: string) => values[key] ?? null,
});
describe('режим навигации', () => {
  it('сохраняет закрепление старого меню при переходе на три режима', () => {
    expect(readNavigationMode(storage({ 'broker:navigation-pinned': 'true' }))).toBe('expanded');
  });
  it('новый выбор имеет приоритет над прежним закреплением', () => {
    expect(
      readNavigationMode(
        storage({ 'broker:navigation-pinned': 'true', 'broker:navigation-mode': 'compact' }),
      ),
    ).toBe('compact');
  });
  it('неизвестное значение и недоступное хранилище не блокируют меню', () => {
    expect(readNavigationMode(storage({ 'broker:navigation-mode': 'invalid' }))).toBe('auto');
    expect(
      readNavigationMode({
        getItem() {
          throw new Error('unavailable');
        },
      }),
    ).toBe('auto');
  });
});
