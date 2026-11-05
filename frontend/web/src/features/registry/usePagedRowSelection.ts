import { useEffect, useState } from 'react';
import { useRowSelection } from './useRowSelection';

// Selected payload is separate from the viewport cache. Resolving a range never pins every page.
export function usePagedRowSelection<T extends { id: string }>(
  page: {
    ids: string[];
    generation: number;
    readRows: (ids: ReadonlySet<string>, signal: AbortSignal) => Promise<T[]>;
  },
  limit: number,
  active = true,
) {
  const selection = useRowSelection(page.ids);
  const [resolved, setResolved] = useState<{
    ids: Set<string>;
    reader: typeof page.readRows;
    generation: number;
    items: T[];
    error?: string;
  }>();
  useEffect(() => {
    if (!active || !selection.selected.size || selection.selected.size > limit) {
      setResolved(undefined);
      return;
    }
    const controller = new AbortController();
    const ids = selection.selected;
    void page
      .readRows(ids, controller.signal)
      .then((items) => {
        if (!controller.signal.aborted)
          setResolved({ ids, reader: page.readRows, generation: page.generation, items });
      })
      .catch((error) => {
        if (!controller.signal.aborted)
          setResolved({
            ids,
            reader: page.readRows,
            generation: page.generation,
            items: [],
            error: (error as Error).message,
          });
      });
    return () => controller.abort();
  }, [selection.selected, page.readRows, page.generation, limit, active]);
  const current =
    resolved?.ids === selection.selected &&
    resolved.reader === page.readRows &&
    resolved.generation === page.generation;
  return {
    ...selection,
    allSelected: page.ids.length > 0 && page.ids.every((id) => selection.selected.has(id)),
    items: current && selection.selected.size <= limit ? resolved.items : [],
    resolving: selection.selected.size > 0 && selection.selected.size <= limit && !current,
    error: current ? resolved.error : undefined,
  };
}
