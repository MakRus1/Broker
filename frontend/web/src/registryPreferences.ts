import { useEffect, useState, type SetStateAction } from 'react';

export function preferenceKey(subject: string, registry: string, kind: string): string {
  return subject ? `broker:registry:${encodeURIComponent(subject)}:${registry}:${kind}:v1` : '';
}

// Ошибка/запрет хранилища не мешает работе реестра. Данные из него всегда проверяются.
export function useRegistryPreference<T>(key: string, initial: T, decode: (raw: unknown) => T) {
  function read(): T {
    try {
      const saved = key && localStorage.getItem(key);
      return saved ? decode(JSON.parse(saved)) : initial;
    } catch {
      return initial;
    }
  }
  const [state, setState] = useState(() => ({ key, value: read() }));
  const value = state.key === key ? state.value : read();
  // Смена пользователя не должна ни записать его предшественника, ни запустить
  // загрузку с прежними фильтрами. React повторит render до commit эффектов.
  if (state.key !== key) setState({ key, value });
  useEffect(() => {
    if (state.key !== key) return;
    try {
      if (key) localStorage.setItem(key, JSON.stringify(state.value));
    } catch {
      /* Личные настройки работают в памяти, если браузер запретил сохранение. */
    }
  }, [key, state]);
  function update(next: SetStateAction<T>) {
    setState((old) => {
      const current = old.key === key ? old.value : read();
      return { key, value: typeof next === 'function' ? (next as (value: T) => T)(current) : next };
    });
  }
  return [value, update] as const;
}

export interface ColumnDefinition {
  id: string;
  label: string;
  width: number;
  required?: boolean;
}
export interface ColumnPreference {
  id: string;
  visible: boolean;
  width: number;
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
      result.push({ id: definition.id, visible: true, width: definition.width });
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
