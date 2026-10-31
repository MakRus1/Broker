export const financialLabelDirections = {
  income: 'Доход',
  expense: 'Расход',
  both: 'Доход и расход',
} as const;
export const financialLabelUnits = {
  container: 'Контейнер',
  transportation: 'Перевозка',
  ton: 'Тонна',
  day: 'Сутки',
  piece: 'Штука',
} as const;
export type VatMode = 'exempt' | 'rate';
export interface FinancialLabel {
  id: string;
  name: string;
  direction: keyof typeof financialLabelDirections;
  unit: keyof typeof financialLabelUnits;
  currency: string;
  vat_mode: VatMode;
  vat_rate_basis_points: number | null;
  note: string | null;
  archived: boolean;
  revision: number;
}
// Десятичный ввод переводится в целое число без промежуточного float.
export function parseVatRate(text: string): number | undefined {
  const match = /^(\d{1,3})(?:[.,](\d{1,2}))?$/.exec(text.trim());
  if (!match) return undefined;
  const value = Number(match[1]) * 100 + Number((match[2] ?? '').padEnd(2, '0'));
  return value <= 10000 ? value : undefined;
}
export function formatVatRate(value: number): string {
  const fraction = String(value % 100)
    .padStart(2, '0')
    .replace(/0+$/, '');
  return `${Math.floor(value / 100)}${fraction ? ',' + fraction : ''}`;
}
export function vatLabel(value: Pick<FinancialLabel, 'vat_mode' | 'vat_rate_basis_points'>) {
  return value.vat_mode === 'exempt'
    ? 'Без НДС'
    : value.vat_rate_basis_points === null
      ? 'Ставка не указана'
      : `${formatVatRate(value.vat_rate_basis_points)}%`;
}
