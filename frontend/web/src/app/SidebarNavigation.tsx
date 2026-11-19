import { useLayoutEffect, useRef, useState, type ReactNode } from 'react';
import { useLocalization } from '../features/localization/Localization';
import './sidebarNavigation.css';

export function SidebarNavigation({ children }: { children: ReactNode }) {
  const { translate } = useLocalization();
  const navigation = useRef<HTMLElement>(null);
  const [edges, setEdges] = useState({ above: false, below: false });
  useLayoutEffect(() => {
    const element = navigation.current;
    if (!element) return;
    function update() {
      const vertical = window.matchMedia('(min-width: 651px)').matches;
      const above = vertical && element!.scrollTop > 1;
      const below =
        vertical && element!.scrollTop + element!.clientHeight < element!.scrollHeight - 1;
      setEdges((old) => (old.above === above && old.below === below ? old : { above, below }));
    }
    const observer = new ResizeObserver(update);
    observer.observe(element);
    for (const child of element.children) observer.observe(child);
    element.addEventListener('scroll', update, { passive: true });
    update();
    return () => {
      observer.disconnect();
      element.removeEventListener('scroll', update);
    };
  }, [children]);
  function scroll(direction: number) {
    const element = navigation.current;
    if (element) element.scrollTop += direction * Math.max(100, element.clientHeight * 0.65);
  }
  return (
    <div className="sidebar-navigation">
      <nav ref={navigation} aria-label={translate('workspace.sections')}>
        {children}
      </nav>
      {(['above', 'below'] as const).map(
        (edge) =>
          edges[edge] && (
            <div key={edge} className={`sidebar-scroll-cue ${edge}`}>
              <button
                type="button"
                onClick={() => scroll(edge === 'above' ? -1 : 1)}
                aria-label={translate(
                  edge === 'above' ? 'workspace.sectionsAbove' : 'workspace.sectionsBelow',
                )}
                title={translate(
                  edge === 'above' ? 'workspace.sectionsAbove' : 'workspace.sectionsBelow',
                )}
              >
                <svg
                  width="16"
                  height="16"
                  viewBox="0 0 24 24"
                  fill="none"
                  stroke="currentColor"
                  strokeWidth="2"
                  aria-hidden="true"
                >
                  <path d={edge === 'above' ? 'm6 15 6-6 6 6' : 'm6 9 6 6 6-6'} />
                </svg>
                <span>
                  {translate(
                    edge === 'above' ? 'workspace.sectionsAbove' : 'workspace.sectionsBelow',
                  )}
                </span>
              </button>
            </div>
          ),
      )}
    </div>
  );
}
