import { useLayoutEffect, useMemo, useRef, useState, type RefObject } from 'react';
import { VIRTUAL_COLUMN_THRESHOLD } from './virtualColumns';
import { rowAt, rowOffsets, visibleRowIndices, VIRTUAL_ROW_THRESHOLD } from './virtualRows';

export function useVirtualRows<T extends { id: string }>(
  items: T[],
  body: RefObject<HTMLTableSectionElement | null>,
  layout: string,
  virtualize = true,
  columnCount = 1,
) {
  const enabled =
    virtualize && (items.length >= VIRTUAL_ROW_THRESHOLD || items.length * columnCount >= 3000);
  const measure = virtualize && (enabled || columnCount >= VIRTUAL_COLUMN_THRESHOLD);
  const updateViewport = useRef<(() => void) | undefined>(undefined);
  const heights = useRef(new Map<string, number>());
  const heightLayouts = useRef(new Map<string, string>());
  const [measurement, setMeasurement] = useState(0);
  const [viewport, setViewport] = useState({
    top: 0,
    height: 800,
    focused: undefined as number | undefined,
  });
  const ids = useMemo(() => items.map((item) => item.id), [items]);
  const offsets = useMemo(() => rowOffsets(ids, heights.current), [ids, measurement]);
  const latest = useRef({ ids, offsets, viewport });
  latest.current = { ids, offsets, viewport };
  useLayoutEffect(() => {
    if (!enabled || !body.current) return;
    const element = body.current;
    let frame = 0;
    function update() {
      frame = 0;
      if (!element.getClientRects().length) return;
      const rect = element.getBoundingClientRect();
      let parent = element.parentElement;
      let top = 0,
        bottom = innerHeight;
      while (parent) {
        const style = getComputedStyle(parent);
        if (/auto|scroll/.test(style.overflowY) && parent.scrollHeight > parent.clientHeight + 1) {
          const bounds = parent.getBoundingClientRect();
          top = Math.max(top, bounds.top);
          bottom = Math.min(bottom, bounds.bottom);
        }
        parent = parent.parentElement;
      }
      const focusedRow =
        document.activeElement?.closest<HTMLTableRowElement>('tr[data-virtual-index]');
      const focused =
        focusedRow?.parentElement === element ? Number(focusedRow.dataset.virtualIndex) : undefined;
      const next = { top: Math.max(0, top - rect.top), height: Math.max(0, bottom - top), focused };
      setViewport((old) =>
        old.top === next.top && old.height === next.height && old.focused === next.focused
          ? old
          : next,
      );
    }
    updateViewport.current = update;
    function schedule() {
      if (!frame) frame = requestAnimationFrame(update);
    }
    const resize = new ResizeObserver(schedule);
    resize.observe(element.parentElement!);
    window.addEventListener('scroll', schedule, true);
    window.addEventListener('resize', schedule);
    document.addEventListener('focusin', schedule);
    update();
    return () => {
      updateViewport.current = undefined;
      cancelAnimationFrame(frame);
      resize.disconnect();
      window.removeEventListener('scroll', schedule, true);
      window.removeEventListener('resize', schedule);
      document.removeEventListener('focusin', schedule);
    };
  }, [enabled, body]);
  // При раскрытии сохранённой вкладки пересчитываем диапазон до отрисовки кадра.
  useLayoutEffect(() => {
    updateViewport.current?.();
  });
  const indices = enabled
    ? visibleRowIndices(offsets, viewport.top, viewport.height, viewport.focused)
    : items.map((_, index) => index);
  const range = indices.join(',');
  useLayoutEffect(() => {
    if (!measure || !body.current) return;
    const element = body.current;
    const observer = new ResizeObserver((entries) => {
      let changed = false;
      let adjustment = 0;
      const current = latest.current;
      const anchor = rowAt(current.offsets, current.viewport.top);
      for (const entry of entries) {
        const row = entry.target as HTMLTableRowElement;
        const index = Number(row.dataset.virtualIndex);
        const id = current.ids[index];
        const height = row.getBoundingClientRect().height;
        if (!id || height <= 0) continue;
        heightLayouts.current.set(id, layout);
        const old = heights.current.get(id) ?? current.offsets[index + 1] - current.offsets[index];
        if (Math.abs(old - height) < 0.5) continue;
        heights.current.set(id, height);
        if (index < anchor) adjustment += height - old;
        changed = true;
      }
      if (changed) {
        if (adjustment && element.getClientRects().length) {
          let parent = element.parentElement;
          while (
            parent &&
            !(
              /auto|scroll/.test(getComputedStyle(parent).overflowY) &&
              parent.scrollHeight > parent.clientHeight + 1
            )
          )
            parent = parent.parentElement;
          if (parent) parent.scrollTop += adjustment;
          else window.scrollBy(0, adjustment);
        }
        setMeasurement((value) => value + 1);
      }
    });
    element.querySelectorAll('tr[data-virtual-index]').forEach((row) => observer.observe(row));
    return () => observer.disconnect();
  }, [measure, body, range, items, layout]);
  // Удалённые записи не накапливают измерения при смене фильтров.
  useLayoutEffect(() => {
    const known = new Set(ids);
    for (const id of heights.current.keys())
      if (!known.has(id)) {
        heights.current.delete(id);
        heightLayouts.current.delete(id);
      }
  }, [ids]);
  return {
    enabled,
    indices,
    offsets,
    retainedHeight: (id: string) =>
      heightLayouts.current.get(id) === layout ? heights.current.get(id) : undefined,
  };
}
