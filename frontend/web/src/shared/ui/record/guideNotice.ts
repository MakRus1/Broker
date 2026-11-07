import type { PreferenceStore } from '../../../features/preferences/preferenceStore';
import { preferenceKey } from '../../../features/registry/registryPreferences';

const kinds = [
  'shipment',
  'counterparty',
  'contract',
  'transportation',
  'service',
  'invoice',
  'receipt',
  'disbursement',
  'label',
];

// Общий проактив знакомит с вопросиком, а не с каждой вкладкой каждой карточки.
export function claimGuideNotice(
  store: Pick<PreferenceStore, 'read' | 'set'>,
  subject: string,
  editing: boolean,
): boolean {
  const mode = editing ? 'edit' : 'view';
  const key = preferenceKey(subject, 'guide-help', 'seen-modes');
  const raw = store.read(key);
  const seen = Array.isArray(raw)
    ? raw.filter((value) => value === 'edit' || value === 'view')
    : [];
  if (seen.includes(mode)) return false;
  const previouslySeen = kinds.some((kind) => {
    const contexts = store.read(preferenceKey(subject, `guide-${kind}`, 'seen-contexts-v3'));
    return (
      Array.isArray(contexts) &&
      contexts.some(
        (context) =>
          typeof context === 'string' &&
          (context.startsWith(`${mode}-`) || context.startsWith(`${mode}:`)),
      )
    );
  });
  // Запоминаем до показа: уход с вкладки раньше таймера не повторяет подсказку.
  store.set(key, [...seen, mode]);
  return !previouslySeen;
}
