import { ru } from "./ru";
import { en } from "./en";
export type MessageKey = keyof typeof ru;
export type Locale = "ru" | "en";
export type LanguagePreference = Locale | "auto";
export function restoreLanguage(value: unknown): LanguagePreference {
  return value === "ru" || value === "en" ? value : "auto";
}
export function resolveLocale(
  preference: LanguagePreference,
  languages: readonly string[],
): Locale {
  if (preference !== "auto") return preference;
  for (const language of languages) {
    const base = language.toLowerCase().split(/[-_]/)[0];
    if (base === "ru" || base === "en") return base;
  }
  return "ru";
}
export function translate(
  locale: Locale,
  key: MessageKey,
  values: Record<string, string | number> = {},
): string {
  const text = { ru, en }[locale][key] ?? ru[key];
  return text.replace(/\{(\w+)\}/g, (placeholder, name: string) =>
    Object.hasOwn(values, name) ? String(values[name]) : placeholder,
  );
}

export function initialLanguagePreference(): LanguagePreference {
  try {
    return restoreLanguage(localStorage.getItem("broker:language-preview"));
  } catch {
    return "auto";
  }
}
