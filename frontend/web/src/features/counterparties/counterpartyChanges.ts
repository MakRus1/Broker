import { translate, type Locale } from '../../../../shared/i18n/catalog';
import { registryValueLabel } from '../registry/registryLabels';
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
export function counterpartyActions(locale: Locale = 'ru'): Record<string, string> {
  return {
    create_counterparty: translate(locale, 'counterparty.counterpartyCreated'),
    update_counterparty: translate(locale, 'counterparty.detailsUpdated'),
    archive_counterparty: translate(locale, 'counterparty.counterpartyArchived'),
    restore_counterparty: translate(locale, 'counterparty.counterpartyRestored'),
  };
}
const fields = {
  name: 'registryField.name',
  legal_name: 'counterparty.legalName',
  country_code: 'registryField.country_code',
  tax_id: 'counterparty.taxIDINN',
  kpp: 'counterparty.kpp',
  registration_id: 'counterparty.registrationNumberOGRN',
  legal_address: 'counterparty.registeredAddress',
  roles: 'registryField.roles',
  contacts: 'counterparty.contacts',
  logist: 'shipment.logist',
  archived: 'registryField.archived',
  own_company: 'registryField.own_company_id',
} as const;
export type HistoryField = keyof typeof fields;
function comparable(snapshot: CounterpartySnapshot | undefined, key: HistoryField): unknown {
  const value = snapshot?.[key];
  // Порядок ролей не является изменением; null и неизвестное значение различаются.
  return key === 'roles' && Array.isArray(value) ? [...value].sort() : value;
}
export function counterpartyChanges(event: CounterpartyEvent, locale: Locale = 'ru') {
  return Object.entries(fields).map(([key, label]) => ({
    key: key as HistoryField,
    label: translate(locale, label),
    changed:
      JSON.stringify(comparable(event.before, key as HistoryField)) !==
      JSON.stringify(comparable(event.after, key as HistoryField)),
  }));
}
export function historyFieldValue(
  snapshot: CounterpartySnapshot | undefined,
  key: HistoryField,
  locale: Locale = 'ru',
): string {
  if (!snapshot || !Object.hasOwn(snapshot, key))
    return translate(locale, 'counterparty.notRecorded');
  if (key === 'own_company')
    return snapshot.own_company ? translate(locale, 'filter.yes') : translate(locale, 'filter.no');
  if (key === 'archived')
    return snapshot.archived
      ? translate(locale, 'filter.archived')
      : translate(locale, 'filter.active');
  if (key === 'roles')
    return (
      snapshot.roles
        ?.map((role) => registryValueLabel('roles', role, locale) ?? counterpartyRoles[role])
        .join(', ') || translate(locale, 'counterparty.notAssigned')
    );
  if (key === 'contacts') {
    return (
      snapshot.contacts
        ?.map((contact) =>
          [contact.name, contact.position, contact.email, contact.phone]
            .filter(Boolean)
            .join(' · '),
        )
        .join('\n') || translate(locale, 'counterparty.noContacts')
    );
  }
  return snapshot[key] || translate(locale, 'shipment.notSpecified');
}
