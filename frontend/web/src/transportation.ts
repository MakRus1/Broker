import { transportationTypes, type Transportation } from './api';

export type RouteSegment = Transportation['route_segments'][number];
export type PlannedRouteSegment = Omit<RouteSegment, 'actual_departure_at' | 'actual_arrival_at'>;

export function localDateTime(value?: string): string {
  if (!value) return '';
  const date = new Date(value);
  if (!Number.isFinite(date.getTime())) return '';
  const pad = (part: number) => String(part).padStart(2, '0');
  return `${String(date.getFullYear()).padStart(4, '0')}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}`;
}
export function utcDateTime(value: string): string {
  const date = new Date(value);
  const parts = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})(?::(\d{2}))?$/.exec(value);
  if (
    !parts ||
    Number(parts[1]) < 1 ||
    !Number.isFinite(date.getTime()) ||
    [
      date.getFullYear(),
      date.getMonth() + 1,
      date.getDate(),
      date.getHours(),
      date.getMinutes(),
      date.getSeconds(),
    ].some((part, index) => part !== Number(parts[index + 1] ?? 0))
  )
    throw new Error('Укажите корректную дату и время');
  return date.toISOString().replace('.000Z', 'Z');
}
export function formatDate(value?: string): string {
  return value
    ? new Date(value).toLocaleString('ru-RU', { dateStyle: 'short', timeStyle: 'short' })
    : '—';
}
export function routeSegment(value: RouteSegment): PlannedRouteSegment {
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
export function routeRequest(id: string, revision: number, route_segments: PlannedRouteSegment[]) {
  if (!route_segments.length || route_segments.length > 32)
    throw new Error('Маршрут должен содержать от 1 до 32 участков');
  if (
    new Set(route_segments.map((route_segment) => route_segment.id)).size !== route_segments.length
  )
    throw new Error('Участки маршрута должны отличаться');
  return {
    transportation_id: id,
    expected_revision: revision,
    route_segments: route_segments.map((route_segment) => {
      const result = routeSegment(route_segment);
      for (const point of [result.origin, result.destination]) {
        point.name = point.name.trim();
        point.country_code = point.country_code.trim().toUpperCase();
        if (point.country_code && !/^[A-Z]{2}$/.test(point.country_code))
          throw new Error('Укажите пункт и двухбуквенный код страны');
      }
      for (const value of [route_segment.planned_departure_at, route_segment.planned_arrival_at]) {
        if (value && !Number.isFinite(new Date(value).getTime()))
          throw new Error('Некорректная плановая дата');
      }
      if (
        route_segment.planned_departure_at &&
        route_segment.planned_arrival_at &&
        new Date(route_segment.planned_departure_at) > new Date(route_segment.planned_arrival_at)
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

export function transportationTitle(
  value: Pick<Transportation, 'container_number' | 'type'>,
): string {
  return value.container_number || `${transportationTypes[value.type]} · Без номера`;
}

export function containerNumberWarning(value: string): string {
  const number = value.trim().toUpperCase();
  if (!number || /^[A-Z]{4}[0-9]{7}$/.test(number)) return '';
  return 'Обычно номер содержит 4 латинские буквы и 7 цифр, например CSQU3054383. Можно сохранить и этот вариант.';
}
