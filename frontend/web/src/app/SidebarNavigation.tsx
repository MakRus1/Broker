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
  return (
    <div className="sidebar-navigation" data-above={edges.above} data-below={edges.below}>
      <nav ref={navigation} aria-label={translate('workspace.sections')}>
        {children}
      </nav>
    </div>
  );
}
