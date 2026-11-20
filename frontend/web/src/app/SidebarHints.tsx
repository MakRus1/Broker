import { useEffect, useId, useRef, useState } from 'react';
import { createPortal } from 'react-dom';

/** Подсказка вынесена из прокручиваемого меню, чтобы его границы её не обрезали. */
export function SidebarHints() {
  const anchor = useRef<HTMLSpanElement>(null);
  const id = useId();
  const [hint, setHint] = useState<{ text: string; left: number; top: number }>();
  useEffect(() => {
    const sidebar = anchor.current?.closest('aside');
    if (!sidebar) return;
    let target: HTMLButtonElement | null = null;
    const clear = () => {
      target?.removeAttribute('aria-describedby');
      target = null;
      setHint(undefined);
    };
    const show = (event: Event) => {
      const button = (event.target as HTMLElement).closest<HTMLButtonElement>('button[aria-label]');
      if (
        !button ||
        button.getAttribute('aria-expanded') === 'true' ||
        !sidebar.contains(button) ||
        sidebar.getBoundingClientRect().width > 80
      ) {
        clear();
        return;
      }
      clear();
      target = button;
      const rect = button.getBoundingClientRect();
      button.setAttribute('aria-describedby', id);
      setHint({
        text: button.getAttribute('aria-label')!,
        left: sidebar.getBoundingClientRect().right + 10,
        top: Math.max(24, Math.min(window.innerHeight - 24, rect.top + rect.height / 2)),
      });
    };
    const leave = (event: Event) => {
      if (target && !target.contains((event as PointerEvent).relatedTarget as Node)) clear();
    };
    const key = (event: KeyboardEvent) => {
      if (event.key === 'Escape') clear();
    };
    const observer = new ResizeObserver(clear);
    observer.observe(sidebar);
    sidebar.addEventListener('pointerover', show);
    sidebar.addEventListener('pointerout', leave);
    sidebar.addEventListener('focusin', show);
    sidebar.addEventListener('focusout', clear);
    sidebar.addEventListener('click', clear);
    sidebar.addEventListener('scroll', clear, true);
    document.addEventListener('keydown', key);
    window.addEventListener('resize', clear);
    return () => {
      observer.disconnect();
      clear();
      sidebar.removeEventListener('pointerover', show);
      sidebar.removeEventListener('pointerout', leave);
      sidebar.removeEventListener('focusin', show);
      sidebar.removeEventListener('focusout', clear);
      sidebar.removeEventListener('click', clear);
      sidebar.removeEventListener('scroll', clear, true);
      document.removeEventListener('keydown', key);
      window.removeEventListener('resize', clear);
    };
  }, [id]);
  return (
    <>
      <span ref={anchor} hidden />
      {hint &&
        createPortal(
          <div
            id={id}
            role="tooltip"
            className="sidebar-hint"
            style={{ left: hint.left, top: hint.top }}
          >
            {hint.text}
          </div>,
          document.body,
        )}
    </>
  );
}
