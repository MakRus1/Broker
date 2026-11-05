import type { MessageKey } from '../../../../shared/i18n/catalog';
import { useEffect, useState, useMemo, useSyncExternalStore, type SetStateAction } from 'react';

import { usePreferences } from '../preferences/UserPreferences';

export function preferenceKey(subject: string, registry: string, kind: string): string {
  return subject ? `broker:registry:${encodeURIComponent(subject)}:${registry}:${kind}:v1` : '';
}

// Ошибка/запрет хранилища не мешает работе реестра. Данные из него всегда проверяются.
export function useRegistryPreference<T>(key: string, initial: T, decode: (raw: unknown) => T) {
  const preferences = usePreferences();
  const store = key ? preferences : null;
  const version = useSyncExternalStore(
    store?.subscribe ?? (() => () => {}),
    store ? () => store.snapshotFor(key) : () => undefined,
    () => undefined,
  );
  function read(): T {
    if (store) {
      const value = store.read(key);
      return value === undefined ? initial : decode(value);
    }
    try {
      const saved = key && localStorage.getItem(key);
      return saved ? decode(JSON.parse(saved)) : initial;
    } catch {
      return initial;
    }
  }
  const [state, setState] = useState(() => ({ key, value: read() }));
  const storedValue = useMemo(() => read(), [key, store, version]);
  const value = store ? storedValue : state.key === key ? state.value : read();
  // Смена пользователя не должна ни записать его предшественника, ни запустить
  // загрузку с прежними фильтрами. React повторит render до commit эффектов.
  if (state.key !== key) setState({ key, value });
  useEffect(() => {
    if (store) {
      store.import(key);
      return;
    }
    if (state.key !== key) return;
    try {
      if (key) localStorage.setItem(key, JSON.stringify(state.value));
    } catch {
      /* Личные настройки работают в памяти, если браузер запретил сохранение. */
    }
  }, [key, state, store, version]);
  function update(next: SetStateAction<T>) {
    if (store) {
      store.set(key, typeof next === 'function' ? (next as (value: T) => T)(read()) : next);
      return;
    }
    setState((old) => {
      const current = old.key === key ? old.value : read();
      return { key, value: typeof next === 'function' ? (next as (value: T) => T)(current) : next };
    });
  }
  return [value, update] as const;
}

export interface ColumnDefinition {
  id: string;
  labelKey: MessageKey;
  width: number;
  required?: boolean;
  defaultVisible?: boolean;
}
export interface ColumnPreference {
  id: string;
  visible: boolean;
  width: number;
}
// Отсутствие списка означает первое знакомство с реестром, а не новые поля.
export function restoreKnownColumns(raw: unknown): string[] | null {
  return Array.isArray(raw) && raw.every((id) => typeof id === 'string') ? [...new Set(raw)] : null;
}
export function newColumnIds(
  known: readonly string[] | null,
  definitions: readonly ColumnDefinition[],
): string[] {
  return known === null
    ? []
    : definitions.filter((column) => !known.includes(column.id)).map((column) => column.id);
}
export function restoreColumns(
  raw: unknown,
  definitions: readonly ColumnDefinition[],
): ColumnPreference[] {
  const saved = Array.isArray(raw) ? raw : [];
  const result: ColumnPreference[] = [];
  for (const entry of saved) {
    const definition = definitions.find((column) => column.id === entry?.id);
    if (!definition || result.some((column) => column.id === definition.id)) continue;
    result.push({
      id: definition.id,
      visible: definition.required || typeof entry.visible !== 'boolean' ? true : entry.visible,
      width:
        typeof entry.width === 'number' && Number.isFinite(entry.width)
          ? Math.max(120, Math.min(900, entry.width))
          : definition.width,
    });
  }
  for (const definition of definitions) {
    if (!result.some((column) => column.id === definition.id))
      result.push({
        id: definition.id,
        visible: definition.required || definition.defaultVisible !== false,
        width: definition.width,
      });
  }
  return result;
}
export function moveColumn(columns: ColumnPreference[], id: string, offset: number) {
  const from = columns.findIndex((column) => column.id === id);
  const to = from + offset;
  if (from < 0 || to < 0 || to >= columns.length) return columns;
  const result = [...columns];
  [result[from], result[to]] = [result[to], result[from]];
  return result;
}

export function placeColumn(
  columns: ColumnPreference[],
  source: string,
  target: string,
  side: 'before' | 'after',
) {
  if (
    source === target ||
    !columns.some((column) => column.id === source) ||
    !columns.some((column) => column.id === target)
  )
    return columns;
  const result = columns.filter((column) => column.id !== source);
  const index = result.findIndex((column) => column.id === target);
  result.splice(
    index + (side === 'after' ? 1 : 0),
    0,
    columns.find((column) => column.id === source)!,
  );
  return result;
}
