import type { DiffRow } from './lineDiff';

export interface DiffSection {
  start: number;
  end: number;
  collapsed: boolean;
}

export function diffSections(rows: DiffRow[], context = 2): DiffSection[] {
  const visible = rows.map(() => false);
  rows.forEach((row, index) => {
    if (row.before?.kind === 'same' && row.after?.kind === 'same') return;
    for (let i = Math.max(0, index - context); i <= Math.min(rows.length - 1, index + context); i++)
      visible[i] = true;
  });
  const sections: DiffSection[] = [];
  for (let start = 0; start < rows.length;) {
    let end = start + 1;
    while (end < rows.length && visible[end] === visible[start]) end++;
    sections.push({ start, end, collapsed: !visible[start] && end - start >= 3 });
    start = end;
  }
  return sections;
}
