import { counterpartyRoles, type Counterparty } from './counterparties';

export type CounterpartySnapshot = Pick<Counterparty, 'name'> &
  Partial<Omit<Counterparty, 'id' | 'name' | 'revision'>>;
export interface CounterpartyEvent {
  id: number;
  action: string;
  actor: string;
  actor_name?: string;
  occurred_at: string;
  revision?: number;
  before?: CounterpartySnapshot;
  after: CounterpartySnapshot;
}
export const counterpartyActions: Record<string, string> = {
  create_counterparty: 'Контрагент создан',
  update_counterparty: 'Изменены реквизиты',
  archive_counterparty: 'Контрагент отправлен в архив',
  restore_counterparty: 'Контрагент восстановлен',
};
const fields = {
  name: 'Наименование',
  legal_name: 'Юридическое наименование',
  country_code: 'Страна',
  tax_id: 'Налоговый номер / ИНН',
  kpp: 'КПП',
  registration_id: 'Регистрационный номер / ОГРН',
  legal_address: 'Юридический адрес',
  roles: 'Роли',
  contacts: 'Контактные лица',
  logist: 'Логист',
  archived: 'Состояние',
  own_company: 'Собственное юрлицо',
} as const;
export type HistoryField = keyof typeof fields;
function comparable(snapshot: CounterpartySnapshot | undefined, key: HistoryField): unknown {
  const value = snapshot?.[key];
  // Порядок ролей не является изменением; null и неизвестное значение различаются.
  return key === 'roles' && Array.isArray(value) ? [...value].sort() : value;
}
export function counterpartyChanges(event: CounterpartyEvent) {
  return (Object.entries(fields) as [HistoryField, string][]).map(([key, label]) => ({
    key,
    label,
    changed:
      JSON.stringify(comparable(event.before, key)) !==
      JSON.stringify(comparable(event.after, key)),
  }));
}
export function historyFieldValue(
  snapshot: CounterpartySnapshot | undefined,
  key: HistoryField,
): string {
  if (!snapshot || !Object.hasOwn(snapshot, key)) return 'Не сохранено';
  if (key === 'own_company') return snapshot.own_company ? 'Да' : 'Нет';
  if (key === 'archived') return snapshot.archived ? 'В архиве' : 'Действующий';
  if (key === 'roles')
    return snapshot.roles?.map((role) => counterpartyRoles[role]).join(', ') || 'Не назначены';
  if (key === 'contacts') {
    return (
      snapshot.contacts
        ?.map((contact) =>
          [contact.name, contact.position, contact.email, contact.phone]
            .filter(Boolean)
            .join(' · '),
        )
        .join('\n') || 'Нет контактов'
    );
  }
  return snapshot[key] || 'Не указано';
}
