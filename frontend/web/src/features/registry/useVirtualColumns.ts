import { useLayoutEffect, useState, type RefObject } from 'react';
import { columnSlots } from './virtualColumns';

export function useVirtualColumns(
  scroll: RefObject<HTMLDivElement | null>,
  widths: readonly number[],
  pinned: readonly number[],
  enabled: boolean,
) {
  const [viewport, setViewport] = useState({ left: 0, width: 1280, focused: -1 });
  useLayoutEffect(() => {
    const element = scroll.current;
    if (!element || !enabled) return;
    let frame = 0;
    function update() {
      frame = 0;
      if (!element!.getClientRects().length) return;
      const cell = document.activeElement?.closest<HTMLElement>('[data-column-index]');
      const next = {
        left: Math.max(
          0,
          element!.scrollLeft - (element!.querySelector('.registry-selection-cell') ? 48 : 0),
        ),
        width: element!.clientWidth,
        focused: cell && element!.contains(cell) ? Number(cell.dataset.columnIndex) : -1,
      };
      setViewport((old) =>
        old.left === next.left && old.width === next.width && old.focused === next.focused
          ? old
          : next,
      );
    }
    function schedule() {
      if (!frame) frame = requestAnimationFrame(update);
    }
    const observer = new ResizeObserver(schedule);
    observer.observe(element);
    element.addEventListener('scroll', schedule, { passive: true });
    element.addEventListener('focusin', schedule);
    element.addEventListener('focusout', schedule);
    update();
    return () => {
      cancelAnimationFrame(frame);
      observer.disconnect();
      element.removeEventListener('scroll', schedule);
      element.removeEventListener('focusin', schedule);
      element.removeEventListener('focusout', schedule);
    };
  }, [enabled, scroll]);
  return enabled
    ? columnSlots(widths, viewport.left, viewport.width, [...pinned, viewport.focused])
    : widths.map((width, index) => ({ index, width }));
}
