import { useEffect, useRef, useState, type PointerEvent as ReactPointerEvent } from 'react';

export function selectRange(
  ids: string[],
  initial: Set<string>,
  from: string,
  to: string,
  checked: boolean,
) {
  const a = ids.indexOf(from),
    b = ids.indexOf(to);
  const result = new Set(initial);
  if (a < 0 || b < 0) return result;
  for (const id of ids.slice(Math.min(a, b), Math.max(a, b) + 1)) {
    if (checked) result.add(id);
    else result.delete(id);
  }
  return result;
}
export function useRowSelection(ids: string[]) {
  const [selected, setSelected] = useState(new Set<string>());
  const [dragging, setDragging] = useState(false);
  const anchor = useRef<string | undefined>(undefined);
  const drag = useRef<
    | {
        start: string;
        initial: Set<string>;
        checked: boolean;
        y: number;
        x: number;
        pointer: number;
      }
    | undefined
  >(undefined);
  const latest = useRef(ids);
  latest.current = ids;
  useEffect(() => {
    if (!dragging) return;
    let frame: number;
    let lastTarget: string | undefined;
    function update() {
      const value = drag.current;
      if (value) {
        const row = document
          .elementFromPoint(value.x, Math.max(1, Math.min(innerHeight - 2, value.y)))
          ?.closest<HTMLElement>('[data-selection-row]');
        if (row?.dataset.selectionRow && row.dataset.selectionRow !== lastTarget) {
          lastTarget = row.dataset.selectionRow;
          setSelected(
            selectRange(
              latest.current,
              value.initial,
              value.start,
              row.dataset.selectionRow,
              value.checked,
            ),
          );
        }
        const delta = value.y < 70 ? -12 : value.y > innerHeight - 70 ? 12 : 0;
        if (delta) {
          const node = document.elementFromPoint(
            value.x,
            Math.max(1, Math.min(innerHeight - 2, value.y)),
          );
          let parent = node?.parentElement;
          while (
            parent &&
            !(
              parent.scrollHeight > parent.clientHeight &&
              /auto|scroll/.test(getComputedStyle(parent).overflowY)
            )
          )
            parent = parent.parentElement;
          if (parent) parent.scrollTop += delta;
          else window.scrollBy(0, delta);
        }
      }
      frame = requestAnimationFrame(update);
    }
    function move(event: PointerEvent) {
      if (drag.current?.pointer === event.pointerId) {
        drag.current.x = event.clientX;
        drag.current.y = event.clientY;
      }
    }
    function end() {
      drag.current = undefined;
      setDragging(false);
    }
    frame = requestAnimationFrame(update);
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', end);
    window.addEventListener('pointercancel', end);
    window.addEventListener('blur', end);
    return () => {
      cancelAnimationFrame(frame);
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', end);
      window.removeEventListener('pointercancel', end);
      window.removeEventListener('blur', end);
    };
  }, [dragging]);
  function toggle(id: string, checked: boolean, shift = false) {
    setSelected((old) =>
      selectRange(ids, old, shift && anchor.current ? anchor.current : id, id, checked),
    );
    anchor.current = id;
  }
  function start(event: ReactPointerEvent, id: string) {
    if (event.button !== 0 || event.pointerType === 'touch') return;
    event.preventDefault();
    const checked = !selected.has(id);
    const from = event.shiftKey && anchor.current ? anchor.current : id;
    drag.current = {
      start: from,
      initial: selected,
      checked,
      x: event.clientX,
      y: event.clientY,
      pointer: event.pointerId,
    };
    setDragging(true);
    toggle(id, checked, event.shiftKey);
  }
  return {
    selected,
    setSelected,
    toggle,
    start,
    clear: () => {
      setDragging(false);
      setSelected(new Set());
      anchor.current = undefined;
      drag.current = undefined;
    },
  };
}
