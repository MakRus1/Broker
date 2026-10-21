import type { Transportation } from './api';

export type Leg = Transportation['legs'][number];
export type RouteLeg = Omit<Leg, 'actual_departure_at' | 'actual_arrival_at'>;

export function localDateTime(value?: string): string {
  if (!value) return '';
  const date = new Date(value);
  if (!Number.isFinite(date.getTime())) return '';
  const local = new Date(date.getTime() - date.getTimezoneOffset() * 60000);
  return local.toISOString().slice(0, 16);
}
export function utcDateTime(value: string): string {
  const date = new Date(value);
  if (!value || !Number.isFinite(date.getTime())) throw new Error('Укажите дату и время');
  return date.toISOString().replace('.000Z', 'Z');
}
export function formatDate(value?: string): string {
  return value
    ? new Date(value).toLocaleString('ru-RU', { dateStyle: 'short', timeStyle: 'short' })
    : '—';
}
export function routeLeg(value: Leg): RouteLeg {
  return {
    id: value.id,
    mode: value.mode,
    origin: { ...value.origin },
    destination: { ...value.destination },
    ...(value.contractor_id ? { contractor_id: value.contractor_id } : {}),
    ...(value.planned_departure_at ? { planned_departure_at: value.planned_departure_at } : {}),
    ...(value.planned_arrival_at ? { planned_arrival_at: value.planned_arrival_at } : {}),
  };
}
export function routeRequest(id: string, revision: number, legs: RouteLeg[]) {
  if (!legs.length || legs.length > 32)
    throw new Error('Маршрут должен содержать от 1 до 32 участков');
  if (new Set(legs.map((leg) => leg.id)).size !== legs.length)
    throw new Error('Участки маршрута должны отличаться');
  return {
    transportation_id: id,
    expected_revision: revision,
    legs: legs.map((leg) => {
      const result = routeLeg(leg);
      for (const point of [result.origin, result.destination]) {
        point.name = point.name.trim();
        point.country_code = point.country_code.trim().toUpperCase();
        if (!point.name || !/^[A-Z]{2}$/.test(point.country_code))
          throw new Error('Укажите пункт и двухбуквенный код страны');
      }
      for (const value of [leg.planned_departure_at, leg.planned_arrival_at]) {
        if (value && !Number.isFinite(new Date(value).getTime()))
          throw new Error('Некорректная плановая дата');
      }
      if (
        leg.planned_departure_at &&
        leg.planned_arrival_at &&
        new Date(leg.planned_departure_at) > new Date(leg.planned_arrival_at)
      )
        throw new Error('Плановое отправление не может быть позже прибытия');
      return result;
    }),
  };
}
export const historyActions: Record<string, string> = {
  set_route: 'Изменён маршрут',
  record_actual: 'Записана фактическая дата',
  correct_actual: 'Исправлена фактическая дата',
};
