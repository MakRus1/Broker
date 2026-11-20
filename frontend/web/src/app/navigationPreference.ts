export type NavigationMode = 'auto' | 'expanded' | 'compact';

export function readNavigationMode(storage?: Pick<Storage, 'getItem'>): NavigationMode {
  try {
    const saved = storage ?? localStorage;
    const mode = saved.getItem('broker:navigation-mode');
    if (mode === 'auto' || mode === 'expanded' || mode === 'compact') return mode;
    return saved.getItem('broker:navigation-pinned') === 'true' ? 'expanded' : 'auto';
  } catch {
    return 'auto';
  }
}
