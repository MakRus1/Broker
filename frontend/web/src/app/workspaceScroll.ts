import { useLayoutEffect, useRef, type RefObject } from 'react';

export type ScrollAnchor = {
  kind: 'field' | 'heading' | 'link';
  text: string;
  occurrence: number;
  top: number;
};
export type ScrollPosition = { y: number; anchor?: ScrollAnchor };
type View = { workspace: string; key: string; editing: boolean };
const normalize = (text: string) =>
  text
    .trim()
    .replace(/\s+/g, ' ')
    .replace(/\s*·\s*[^·]*$/, '');
const visible = (element: HTMLElement) => element.getClientRects().length > 0;

function anchors(root: HTMLElement) {
  return Array.from(root.querySelectorAll<HTMLElement>('label,dt,.field-label,h2,h3,a[href]'))
    .filter(
      (node) =>
        visible(node) &&
        !node.closest('.tabs,.record-editor-sections,.record-layout-save,.lookup-field-popup'),
    )
    .map((node) => ({
      node,
      kind: (node.matches('h2,h3')
        ? 'heading'
        : node.matches('a')
          ? 'link'
          : 'field') as ScrollAnchor['kind'],
      text: node.matches('a')
        ? (node.getAttribute('href') ?? '')
        : normalize(
            Array.from(node.childNodes)
              .filter((child) => child.nodeType === Node.TEXT_NODE)
              .map((child) => child.textContent)
              .join(' ') ||
              node.textContent ||
              '',
          ),
    }))
    .filter((entry) => entry.text);
}
function snapshot(root: HTMLElement): ScrollPosition {
  const entries = anchors(root);
  // Prefer a nearby field or heading over a raw pixel offset, which changes between edit and view.
  const entry = entries
    .filter(({ node }) => node.getBoundingClientRect().top < innerHeight - 100)
    .sort(
      (a, b) =>
        Math.abs(a.node.getBoundingClientRect().top - 100) -
        Math.abs(b.node.getBoundingClientRect().top - 100),
    )[0];
  return {
    y: window.scrollY,
    anchor: entry
      ? {
          kind: entry.kind,
          text: entry.text,
          occurrence: entries
            .filter((other) => other.kind === entry.kind && other.text === entry.text)
            .findIndex((other) => other.node === entry.node),
          top: entry.node.getBoundingClientRect().top,
        }
      : undefined,
  };
}
function view(root: HTMLElement, workspace: string): View {
  const tabs = Array.from(root.querySelectorAll<HTMLElement>('.tabs'))
    .filter(visible)
    .map((bar) =>
      Array.from(bar.querySelectorAll('[aria-pressed],[role="tab"]')).findIndex(
        (button) =>
          button.getAttribute('aria-pressed') === 'true' ||
          button.getAttribute('aria-selected') === 'true',
      ),
    );
  const editing = Array.from(root.querySelectorAll<HTMLElement>('.record-layout-save')).some(
    visible,
  );
  return { workspace, editing, key: JSON.stringify([workspace, editing, tabs]) };
}
export function destinationPosition(
  saved: ScrollPosition | undefined,
  previous: ScrollPosition | undefined,
  sameWorkspace: boolean,
  modeChanged: boolean,
): ScrollPosition {
  // Saving or cancelling an edit returns to the edited block, rather than an older view position.
  return (
    (sameWorkspace && modeChanged ? previous : saved) ??
    (sameWorkspace ? previous : undefined) ?? { y: 0 }
  );
}

export function useWorkspaceScroll(
  rootRef: RefObject<HTMLDivElement | null>,
  workspace: string,
  scope: string,
) {
  const state = useRef<{
    scope: string;
    positions: Map<string, ScrollPosition>;
    current?: View;
    position?: ScrollPosition;
  }>({ scope, positions: new Map() });
  useLayoutEffect(() => {
    const root = rootRef.current;
    if (!root) return;
    if (state.current.scope !== scope) state.current = { scope, positions: new Map() };
    const memory = state.current;
    const nativeRestoration = history.scrollRestoration;
    history.scrollRestoration = 'manual';
    let restore: (() => void) | undefined;
    let frame = 0;
    let timeout = 0;
    let suppressedUntil = 0;
    let disposed = false;
    let navigationFocus = false;
    const isNavigation = (target: EventTarget | null) =>
      target instanceof Element && !!target.closest('.tabs button,.workspace-tabbar [role="tab"]');
    const onFocus = (event: FocusEvent) => {
      navigationFocus = isNavigation(event.target);
    };
    const remember = () => {
      if (!memory.current) return;
      memory.position = snapshot(root);
      memory.positions.set(memory.current.key, memory.position);
    };
    const stop = () => {
      restore = undefined;
      clearTimeout(timeout);
    };
    const restorePosition = (position: ScrollPosition) => {
      stop();
      restore = () => {
        const anchor = position.anchor;
        const target =
          anchor &&
          anchors(root).filter((entry) => entry.kind === anchor.kind && entry.text === anchor.text)[
            anchor.occurrence
          ]?.node;
        const y =
          target && anchor
            ? window.scrollY + target.getBoundingClientRect().top - anchor.top
            : position.y;
        window.scrollTo({ top: Math.max(0, y), left: 0, behavior: 'instant' });
        if (Math.abs(window.scrollY - Math.max(0, y)) <= 1) stop();
      };
      // Content can arrive after the navigation commit. ResizeObserver retries only while restoring,
      // and any user interaction or explicit programmatic navigation cancels this bounded window.
      timeout = window.setTimeout(() => {
        stop();
        remember();
      }, 10000);
      restore?.();
    };
    const scan = () => {
      if (disposed) return;
      const next = view(root, workspace);
      const previous = memory.current;
      if (next.key !== previous?.key) {
        const position = destinationPosition(
          memory.positions.get(next.key),
          memory.position,
          previous?.workspace === workspace,
          !!previous && previous.editing !== next.editing,
        );
        memory.current = next;
        memory.position = position;
        memory.positions.set(next.key, position);
        if (performance.now() >= suppressedUntil) restorePosition(position);
      } else restore?.();
    };
    const schedule = () => {
      if (!frame)
        frame = requestAnimationFrame(() => {
          frame = 0;
          scan();
        });
    };
    const beforeNavigation = (event: Event) => {
      stop();
      // Focusing an off-screen tab can scroll it into view before activation.
      // Keep the last reading position instead of recording that browser adjustment.
      if (!isNavigation(event.target)) remember();
    };
    const userScroll = () => {
      navigationFocus = false;
      stop();
    };
    const explicit = () => {
      stop();
      suppressedUntil = performance.now() + 500;
    };
    const onScroll = (event: Event) => {
      if (event.target !== document || restore || navigationFocus) return;
      if (view(root, workspace).key !== memory.current?.key) return;
      // Do not read the entire form on every scroll tick. Capture the semantic anchor before navigation.
      if (memory.current) {
        memory.position = { y: window.scrollY };
        memory.positions.set(memory.current.key, memory.position);
      }
    };
    const onKey = (event: KeyboardEvent) => {
      if (['Enter', ' ', 'Tab'].includes(event.key)) beforeNavigation(event);
      else if (['ArrowDown', 'ArrowUp', 'PageDown', 'PageUp', 'Home', 'End'].includes(event.key))
        userScroll();
    };
    const observer = new MutationObserver(schedule);
    observer.observe(root, {
      subtree: true,
      childList: true,
      attributes: true,
      attributeFilter: ['hidden', 'aria-pressed', 'aria-selected'],
    });
    const resize = new ResizeObserver(() => {
      if (restore) schedule();
    });
    resize.observe(root);
    document.addEventListener('focusin', onFocus, true);
    document.addEventListener('click', beforeNavigation, true);
    document.addEventListener('submit', beforeNavigation, true);
    document.addEventListener('keydown', onKey, true);
    window.addEventListener('wheel', userScroll, { passive: true });
    window.addEventListener('touchstart', userScroll, { passive: true });
    window.addEventListener('scroll', onScroll, { passive: true });
    window.addEventListener('workspace-scroll-intent', explicit);
    scan();
    return () => {
      history.scrollRestoration = nativeRestoration;
      disposed = true;
      stop();
      cancelAnimationFrame(frame);
      observer.disconnect();
      resize.disconnect();
      document.removeEventListener('focusin', onFocus, true);
      document.removeEventListener('click', beforeNavigation, true);
      document.removeEventListener('submit', beforeNavigation, true);
      document.removeEventListener('keydown', onKey, true);
      window.removeEventListener('wheel', userScroll);
      window.removeEventListener('touchstart', userScroll);
      window.removeEventListener('scroll', onScroll);
      window.removeEventListener('workspace-scroll-intent', explicit);
    };
  }, [rootRef, workspace, scope]);
}
