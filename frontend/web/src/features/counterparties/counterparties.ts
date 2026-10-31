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
  own_company?: boolean;
  legal_name: string | null;
  country_code: string | null;
  tax_id: string | null;
  kpp?: string | null;
  logist?: string | null;
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
  own_company: boolean;
  name: string;
  legal_name: string;
  country_code: string;
  tax_id: string;
  kpp: string;
  logist: string;
  registration_id: string;
  legal_address: string;
  roles: CounterpartyRole[];
  contacts: CounterpartyContactDraft[];
}
export function counterpartyDetails(value?: Counterparty): CounterpartyDetails {
  return {
    own_company: value?.own_company ?? false,
    name: value?.name ?? '',
    legal_name: value?.legal_name ?? '',
    country_code: value?.country_code ?? '',
    tax_id: value?.tax_id ?? '',
    kpp: value?.kpp ?? '',
    logist: value?.logist ?? '',
    registration_id: value?.registration_id ?? '',
    legal_address: value?.legal_address ?? '',
    roles: [...(value?.roles ?? [])],
    contacts: (value?.contacts ?? []).map((contact) => ({
      name: contact.name.trim(),
      position: contact.position ?? '',
      email: contact.email ?? '',
      phone: contact.phone ?? '',
    })),
  };
}
// UI помогает вводить реквизиты; сервис независимо применяет те же правила.
function nullableField(value: string): string | null {
  return value.trim() || null;
}
export function counterpartyPayload(
  value: CounterpartyDetails,
): Omit<Counterparty, 'id' | 'revision' | 'archived'> {
  return {
    ...value,
    name: value.name.trim(),
    legal_name: nullableField(value.legal_name),
    country_code: nullableField(value.country_code)?.toUpperCase() ?? null,
    tax_id: nullableField(value.tax_id),
    kpp: nullableField(value.kpp),
    logist: nullableField(value.logist),
    registration_id: nullableField(value.registration_id),
    legal_address: nullableField(value.legal_address),
    contacts: value.contacts.map((contact) => ({
      name: contact.name.trim(),
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
    if (fields.kpp !== undefined && typeof fields.kpp !== 'string') return fallback;
    return {
      ...candidate,
      value: {
        ...fields,
        own_company: fields.own_company === true,
        kpp: fields.kpp ?? '',
        logist: typeof fields.logist === 'string' ? fields.logist : '',
      },
    } as unknown as CounterpartyDraft;
  } catch {
    return fallback;
  }
}

export function isEmail(value: string): boolean {
  const parts = value.split('@');
  if (parts.length !== 2 || value.length > 254) return false;
  const [local, domain] = parts;
  if (
    !local ||
    local.length > 64 ||
    local.startsWith('.') ||
    local.endsWith('.') ||
    local.includes('..') ||
    /[\s<>(),;:"\\[\]]/.test(local)
  )
    return false;
  const labels = domain.split('.');
  return (
    labels.length >= 2 &&
    labels.every(
      (label) =>
        label.length > 0 &&
        label.length <= 63 &&
        !label.startsWith('-') &&
        !label.endsWith('-') &&
        Array.from(label).every((char) => char.charCodeAt(0) >= 128 || /^[A-Za-z0-9-]$/.test(char)),
    )
  );
}

export function isPhone(value: string): boolean {
  let digits = 0;
  let opened = false;
  for (let index = 0; index < value.length; index++) {
    const char = value[index];
    if (/^[0-9]$/.test(char)) digits++;
    else if (char === '(' && !opened) opened = true;
    else if (char === ')' && opened) opened = false;
    else if (!' -.'.includes(char) && !(char === '+' && index === 0)) return false;
  }
  return !opened && digits >= 7 && digits <= 15;
}

export function validateContacts(contacts: CounterpartyContactDraft[]) {
  for (const [index, contact] of contacts.entries()) {
    if (contact.email.trim() && !isEmail(contact.email.trim()))
      throw new Error(`Контакт ${index + 1}: проверьте email`);
    if (contact.phone.trim() && !isPhone(contact.phone.trim()))
      throw new Error(`Контакт ${index + 1}: телефон должен содержать от 7 до 15 цифр`);
  }
}
