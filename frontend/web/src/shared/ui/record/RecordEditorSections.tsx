import { scrollIntent } from '../scrollIntent';
import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useRef,
  useState,
  type ReactNode,
} from 'react';
import { useLocalization } from '../../../features/localization/Localization';

type Entry = { id: string; title: string };
const Sections = createContext<((entry: Entry) => () => void) | null>(null);

export function useEditorSection(id: string, title: string) {
  const register = useContext(Sections);
  useEffect(() => register?.({ id, title }), [register, id, title]);
}

export function activeEditorSection(sections: { id: string; top: number }[], boundary: number) {
  let current = sections[0]?.id;
  for (const section of sections) {
    if (section.top > boundary) break;
    current = section.id;
  }
  return current;
}

export function RecordEditorSections({ children }: { children: ReactNode }) {
  const { translate } = useLocalization();
  const [entries, setEntries] = useState<Entry[]>([]);
  const [active, setActive] = useState<string>();
  const selected = useRef<{ id: string; limit: number } | null>(null);
  const nav = useRef<HTMLElement>(null);
  const register = useCallback((entry: Entry) => {
    setEntries((old) => [...old.filter((value) => value.id !== entry.id), entry]);
    return () => setEntries((old) => old.filter((value) => value.id !== entry.id));
  }, []);
  useEffect(() => {
    if (entries.length < 3) return;
    let frame = 0;
    function update() {
      frame = 0;
      if (!nav.current || nav.current.getClientRects().length === 0) return;
      const sections = entries.flatMap((entry) => {
        const node = document.getElementById(entry.id);
        return node?.getClientRects().length
          ? [{ id: entry.id, top: node.getBoundingClientRect().top }]
          : [];
      });
      const viewport = document.scrollingElement;
      const limit = viewport ? viewport.scrollHeight - viewport.clientHeight : 0;
      if (
        selected.current &&
        window.scrollY >= selected.current.limit - 2 &&
        limit <= selected.current.limit + 2
      ) {
        setActive(selected.current.id);
        return;
      }
      selected.current = null;
      const boundary = nav.current.getBoundingClientRect().bottom + 36;
      setActive(activeEditorSection(sections, boundary));
    }
    function schedule() {
      if (!frame) frame = requestAnimationFrame(update);
    }
    update();
    window.addEventListener('scroll', schedule, { passive: true });
    window.addEventListener('resize', schedule);
    return () => {
      cancelAnimationFrame(frame);
      window.removeEventListener('scroll', schedule);
      window.removeEventListener('resize', schedule);
    };
  }, [entries]);
  return (
    <Sections.Provider value={register}>
      {entries.length >= 3 && (
        <nav ref={nav} className="record-editor-sections" aria-label={translate('record.sections')}>
          {entries.map((entry) => (
            <button
              key={entry.id}
              type="button"
              aria-current={active === entry.id ? 'location' : undefined}
              onClick={() => {
                const node = document.getElementById(entry.id);
                if (!node) return;
                const viewport = document.scrollingElement;
                const limit = viewport ? viewport.scrollHeight - viewport.clientHeight : 0;
                const margin = parseFloat(getComputedStyle(node).scrollMarginTop) || 0;
                const target = node.getBoundingClientRect().top + window.scrollY - margin;
                selected.current = target >= limit ? { id: entry.id, limit } : null;
                setActive(entry.id);
                scrollIntent();
                node.scrollIntoView({
                  block: 'start',
                  behavior:
                    selected.current ||
                    window.matchMedia('(prefers-reduced-motion: reduce)').matches
                      ? 'instant'
                      : 'smooth',
                });
                node.querySelector<HTMLElement>('h2')?.focus({ preventScroll: true });
              }}
            >
              {entry.title}
            </button>
          ))}
        </nav>
      )}
      {children}
    </Sections.Provider>
  );
}
