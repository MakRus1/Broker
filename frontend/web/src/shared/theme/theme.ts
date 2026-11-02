export type ThemePreference = 'light' | 'dark' | 'system';
export function restoreTheme(value: unknown): ThemePreference {
  return value === 'light' || value === 'dark' ? value : 'system';
}
export function resolveTheme(preference: ThemePreference, systemDark: boolean): 'light' | 'dark' {
  return preference === 'system' ? (systemDark ? 'dark' : 'light') : preference;
}

export function initialThemePreference(): ThemePreference {
  let preference: ThemePreference = 'system';
  try {
    preference = restoreTheme(localStorage.getItem('broker:theme-preview'));
  } catch {
    /* Хранилище может быть недоступно. */
  }
  return preference;
}
export function applyInitialTheme() {
  document.documentElement.dataset.theme = resolveTheme(
    initialThemePreference(),
    window.matchMedia('(prefers-color-scheme: dark)').matches,
  );
}
