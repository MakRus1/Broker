import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { formatFixed } from '../services/serviceCharges';
export type HistoryKind = 'service-charges' | 'invoices' | 'payments';
export interface HistoryValue {
  value: string;
  format: 'text' | 'date' | 'money' | 'quantity' | 'percent' | 'rate';
  currency?: string;
}
export interface HistoryCursor {
  source: 'service_charge' | 'invoice' | 'payment';
  entity_id: string;
  revision: number;
  occurred_at: string;
}
export interface HistoryEvent {
  cursor: HistoryCursor;
  actor: string;
  actor_name?: string;
  title: string;
  has_previous: boolean;
  changes: { field: string; label: string; before?: HistoryValue; after?: HistoryValue }[];
}
export interface HistoryPage {
  items: HistoryEvent[];
  next_before?: HistoryCursor;
}
export function historyValue(value?: HistoryValue): string {
  if (!value) return '—';
  switch (value.format) {
    case 'date':
      try {
        return CalendarDate.fromISO(value.value).toDisplayString();
      } catch {
        return value.value;
      }
    case 'money':
      return `${formatFixed(value.value, 2)}${value.currency ? ' ' + value.currency : ''}`;
    case 'quantity':
      return formatFixed(value.value, 3);
    case 'percent':
      return (
        formatFixed(value.value, 2)
          .replace(/,00$/, '')
          .replace(/(,\d)0$/, '$1') + '%'
      );
    case 'rate':
      return formatFixed(value.value, 6).replace(/,?0+$/, '');
    default:
      return value.value;
  }
}
