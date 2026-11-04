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
  const text =
    publishedMessages[locale]?.[key] ?? { ru, en }[locale][key] ?? ru[key];
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

export type PublishedMessages = Partial<Record<MessageKey, string>>;
let publishedMessages: Partial<Record<Locale, PublishedMessages>> = {};

export function replacePublishedMessages(
  locale: Locale,
  messages: PublishedMessages,
) {
  publishedMessages = { [locale]: messages };
}

export function clearPublishedMessages() {
  publishedMessages = {};
}

export function validatePublishedMessages(
  value: unknown,
  locale: Locale,
): PublishedMessages {
  if (!Array.isArray(value) || value.length > 10000) return {};
  const result: PublishedMessages = {};
  const source = { ru, en }[locale];
  for (const item of value) {
    if (
      !item ||
      typeof item.key !== "string" ||
      !Object.hasOwn(source, item.key) ||
      typeof item.value !== "string" ||
      !item.value.trim() ||
      item.value.length > 10000
    )
      continue;
    const key = item.key as MessageKey;
    const parameters = (text: string) =>
      [...new Set(text.match(/\{\w+\}/g) ?? [])].sort().join(",");
    if (parameters(source[key]) === parameters(item.value))
      result[key] = item.value;
  }
  return result;
}
