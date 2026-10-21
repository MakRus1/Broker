export const counterpartyRoles = {
  customer: 'Клиент',
  carrier: 'Перевозчик',
  agent: 'Агент',
  supplier: 'Поставщик',
  partner: 'Партнёр',
} as const;
export type CounterpartyRole = keyof typeof counterpartyRoles;
export interface CounterpartyContact {
  name: string;
  position: string | null;
  email: string | null;
  phone: string | null;
}
export interface Counterparty {
  id: string;
  name: string;
  revision: number;
  archived: boolean;
  legal_name: string | null;
  country_code: string | null;
  tax_id: string | null;
  registration_id: string | null;
  legal_address: string | null;
  roles: CounterpartyRole[];
  contacts: CounterpartyContact[];
}
export interface CounterpartyContactDraft {
  name: string;
  position: string;
  email: string;
  phone: string;
}
export interface CounterpartyDetails {
  name: string;
  legal_name: string;
  country_code: string;
  tax_id: string;
  registration_id: string;
  legal_address: string;
  roles: CounterpartyRole[];
  contacts: CounterpartyContactDraft[];
}
export function counterpartyDetails(value?: Counterparty): CounterpartyDetails {
  return {
    name: value?.name ?? '',
    legal_name: value?.legal_name ?? '',
    country_code: value?.country_code ?? '',
    tax_id: value?.tax_id ?? '',
    registration_id: value?.registration_id ?? '',
    legal_address: value?.legal_address ?? '',
    roles: [...(value?.roles ?? [])],
    contacts: (value?.contacts ?? []).map((contact) => ({
      name: contact.name,
      position: contact.position ?? '',
      email: contact.email ?? '',
      phone: contact.phone ?? '',
    })),
  };
}
// Форма отвечает только за представление пустого поля; правила реквизитов — на сервере.
function nullableField(value: string): string | null {
  return value.trim() === '' ? null : value;
}
export function counterpartyPayload(
  value: CounterpartyDetails,
): Omit<Counterparty, 'id' | 'revision' | 'archived'> {
  return {
    ...value,
    legal_name: nullableField(value.legal_name),
    country_code: nullableField(value.country_code),
    tax_id: nullableField(value.tax_id),
    registration_id: nullableField(value.registration_id),
    legal_address: nullableField(value.legal_address),
    contacts: value.contacts.map((contact) => ({
      name: contact.name,
      position: nullableField(contact.position),
      email: nullableField(contact.email),
      phone: nullableField(contact.phone),
    })),
  };
}

export interface CounterpartyDraft {
  id: string;
  baseRevision: number;
  value: CounterpartyDetails;
}
export function restoreCounterpartyDraft(
  raw: string | null,
  fallback: CounterpartyDraft,
): CounterpartyDraft {
  try {
    const stored: unknown = JSON.parse(raw || 'null');
    if (!stored || typeof stored !== 'object') return fallback;
    const candidate = stored as Record<string, unknown>;
    const value = candidate.value;
    if (
      typeof candidate.id !== 'string' ||
      !/^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$/i.test(candidate.id) ||
      !Number.isSafeInteger(candidate.baseRevision) ||
      (candidate.baseRevision as number) < 0 ||
      !value ||
      typeof value !== 'object'
    )
      return fallback;
    const fields = value as Record<string, unknown>;
    for (const key of [
      'name',
      'legal_name',
      'country_code',
      'tax_id',
      'registration_id',
      'legal_address',
    ]) {
      if (typeof fields[key] !== 'string') return fallback;
    }
    if (
      !Array.isArray(fields.roles) ||
      fields.roles.some(
        (role) => typeof role !== 'string' || !Object.hasOwn(counterpartyRoles, role),
      ) ||
      !Array.isArray(fields.contacts) ||
      fields.contacts.length > 32 ||
      fields.contacts.some(
        (contact) =>
          !contact ||
          typeof contact !== 'object' ||
          ['name', 'position', 'email', 'phone'].some((key) => typeof contact[key] !== 'string'),
      )
    )
      return fallback;
    return candidate as unknown as CounterpartyDraft;
  } catch {
    return fallback;
  }
}
