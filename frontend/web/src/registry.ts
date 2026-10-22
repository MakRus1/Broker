import type { CounterpartyDetails } from './counterparties';

export interface RegistryParty {
  name: string;
  legal_name: string | null;
  tax_id: string;
  kpp: string | null;
  registration_id: string | null;
  legal_address: string | null;
  status: string | null;
}
export const registryFields = {
  name: 'Краткое наименование',
  legal_name: 'Юридическое наименование',
  country_code: 'Страна',
  tax_id: 'ИНН',
  kpp: 'КПП',
  registration_id: 'ОГРН / ОГРНИП',
  legal_address: 'Юридический адрес',
} as const;
export type RegistryRequisites = Pick<CounterpartyDetails, keyof typeof registryFields>;
export function registryRequisites(value: RegistryParty): RegistryRequisites {
  return {
    name: value.name,
    legal_name: value.legal_name ?? '',
    country_code: 'RU',
    tax_id: value.tax_id,
    kpp: value.kpp ?? '',
    registration_id: value.registration_id ?? '',
    legal_address: value.legal_address ?? '',
  };
}
export function isInn(value: string): boolean {
  if (!/^(\d{10}|\d{12})$/.test(value) || /^0+$/.test(value)) return false;
  const check = (weights: number[], digit: number) =>
    (weights.reduce((sum, weight, index) => sum + weight * Number(value[index]), 0) % 11) % 10 ===
    Number(value[digit]);
  return value.length === 10
    ? check([2, 4, 10, 3, 5, 9, 4, 6, 8], 9)
    : check([7, 2, 4, 10, 3, 5, 9, 4, 6, 8], 10) && check([3, 7, 2, 4, 10, 3, 5, 9, 4, 6, 8], 11);
}
export function registryStatus(value: string | null): string {
  const labels: Record<string, string> = {
    ACTIVE: 'Действующая',
    LIQUIDATING: 'В процессе ликвидации',
    LIQUIDATED: 'Ликвидирована',
    BANKRUPT: 'Банкротство',
    REORGANIZING: 'В процессе реорганизации',
  };
  return value ? (labels[value] ?? value) : 'Нет сведений о статусе';
}
