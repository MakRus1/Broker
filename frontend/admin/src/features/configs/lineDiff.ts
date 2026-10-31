export interface DiffCell {
  text: string;
  line: number;
  kind: 'same' | 'removed' | 'added';
}
export interface DiffRow {
  before?: DiffCell;
  after?: DiffCell;
}

function ordered(value: unknown): unknown {
  if (Array.isArray(value)) return value.map(ordered);
  if (value !== null && typeof value === 'object') {
    return Object.fromEntries(
      Object.entries(value)
        .sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0))
        .map(([key, child]) => [key, ordered(child)]),
    );
  }
  return value;
}

export function valueLines(value: unknown): string[] {
  return JSON.stringify(ordered(value), null, 2).split('\n');
}

export function lineDiff(before: unknown, after: unknown): DiffRow[] {
  const left = valueLines(before);
  const right = valueLines(after);
  const rows: DiffRow[] = [];
  let i = 0;
  let j = 0;
  function block(endLeft: number, endRight: number) {
    while (i < endLeft || j < endRight) {
      rows.push({
        before: i < endLeft ? { text: left[i], line: ++i, kind: 'removed' } : undefined,
        after: j < endRight ? { text: right[j], line: ++j, kind: 'added' } : undefined,
      });
    }
  }
  // Ограничиваем память сравнения для больших документов; сохраняем общие начало и конец.
  if (left.length * right.length > 1_000_000) {
    while (i < left.length && j < right.length && left[i] === right[j]) {
      rows.push({
        before: { text: left[i], line: ++i, kind: 'same' },
        after: { text: right[j], line: ++j, kind: 'same' },
      });
    }
    let suffix = 0;
    while (
      left.length - suffix > i &&
      right.length - suffix > j &&
      left[left.length - suffix - 1] === right[right.length - suffix - 1]
    )
      suffix++;
    block(left.length - suffix, right.length - suffix);
  } else {
    const width = right.length + 1;
    const lengths = new Uint32Array((left.length + 1) * width);
    for (let a = left.length - 1; a >= 0; a--) {
      for (let b = right.length - 1; b >= 0; b--) {
        lengths[a * width + b] =
          left[a] === right[b]
            ? 1 + lengths[(a + 1) * width + b + 1]
            : Math.max(lengths[(a + 1) * width + b], lengths[a * width + b + 1]);
      }
    }
    while (i < left.length && j < right.length) {
      if (left[i] === right[j]) {
        rows.push({
          before: { text: left[i], line: ++i, kind: 'same' },
          after: { text: right[j], line: ++j, kind: 'same' },
        });
        continue;
      }
      let a = i;
      let b = j;
      while (a < left.length && b < right.length && left[a] !== right[b]) {
        if (lengths[(a + 1) * width + b] >= lengths[a * width + b + 1]) a++;
        else b++;
      }
      block(a, b);
    }
    block(left.length, right.length);
  }
  while (i < left.length && j < right.length) {
    rows.push({
      before: { text: left[i], line: ++i, kind: 'same' },
      after: { text: right[j], line: ++j, kind: 'same' },
    });
  }
  return rows;
}
