import { parseValue } from './api';

export interface Change {
  path: string;
  before?: unknown;
  after?: unknown;
  kind: 'added' | 'removed' | 'changed';
}

function object(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

export function changes(before: unknown, after: unknown, path = '$'): Change[] {
  if (Object.is(before, after)) return [];
  if (object(before) && object(after)) {
    return [...new Set([...Object.keys(before), ...Object.keys(after)])].sort().flatMap((key) => {
      const child = `${path}[${JSON.stringify(key)}]`;
      if (!Object.hasOwn(before, key))
        return [{ path: child, after: after[key], kind: 'added' as const }];
      if (!Object.hasOwn(after, key))
        return [{ path: child, before: before[key], kind: 'removed' as const }];
      return changes(before[key], after[key], child);
    });
  }
  if (Array.isArray(before) && Array.isArray(after)) {
    return Array.from({ length: Math.max(before.length, after.length) }, (_, index) => {
      const child = `${path}[${index}]`;
      if (index >= before.length)
        return [{ path: child, after: after[index], kind: 'added' as const }];
      if (index >= after.length)
        return [{ path: child, before: before[index], kind: 'removed' as const }];
      return changes(before[index], after[index], child);
    }).flat();
  }
  return [{ path, before, after, kind: 'changed' }];
}

export function editorState(text: string, initial: unknown, published: unknown) {
  try {
    const value = parseValue(text);
    const diff = changes(published, value);
    return { diff, changed: changes(initial, value).length > 0 && diff.length > 0, error: '' };
  } catch (reason) {
    return {
      diff: [],
      changed: true,
      error:
        reason instanceof SyntaxError
          ? 'Некорректный JSON. Проверьте кавычки, запятые и скобки.'
          : (reason as Error).message,
    };
  }
}
