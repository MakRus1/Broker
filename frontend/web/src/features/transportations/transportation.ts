import { transportationTypes } from '../routes/logisticsLabels';
import { type Transportation } from './types';

export type RouteSegment = Transportation['route_segments'][number];
export type PlannedRouteSegment = Omit<RouteSegment, 'actual_departure_at' | 'actual_arrival_at'>;

// Вычисляем подписи, не копируя станции в свободные пункты маршрута.
export function routePoint(segment: RouteSegment, point: 'origin' | 'destination') {
  if (segment.mode === 'sea')
    return (
      (point === 'origin' ? segment.sea?.pol : segment.sea?.pod) ?? { name: '', country_code: '' }
    );
  if (segment.mode === 'air')
    return (
      (point === 'origin' ? segment.air?.origin_airport : segment.air?.destination_airport) ?? {
        name: '',
        country_code: '',
      }
    );
  const station =
    point === 'origin' ? segment.rail?.origin_station : segment.rail?.destination_station;
  if (segment.mode === 'rail' || segment.mode === 'international_rail')
    return station ?? { name: '', country_code: '' };
  if (
    (segment.mode === 'road' || segment.mode === 'pickup') &&
    point === 'destination' &&
    segment.pickup
  )
    return {
      name: segment.pickup.delivery_city ?? '',
      country_code: segment.pickup.delivery_country ?? '',
    };
  return segment[point] ?? { name: '', country_code: '' };
}

export function routeDirection(segment: RouteSegment): string {
  if (segment.mode === 'international_road')
    return segment.international_road?.loading_terminal_id
      ? 'Место погрузки указано'
      : 'Место погрузки не указано';
  if (segment.mode === 'road' || segment.mode === 'pickup')
    return (
      segment.pickup?.delivery_city ||
      segment.pickup?.delivery_address ||
      'Место доставки не указано'
    );
  const fallback =
    segment.mode === 'rail' || segment.mode === 'international_rail'
      ? 'Станция не выбрана'
      : 'Пункт не указан';
  return `${routePoint(segment, 'origin').name || fallback} → ${routePoint(segment, 'destination').name || fallback}`;
}

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
    ...(value.rail
      ? { rail: { ...value.rail } }
      : (value.mode === 'rail' || value.mode === 'international_rail') &&
          (value.planned_departure_at ||
            value.planned_arrival_at ||
            value.actual_departure_at ||
            value.actual_arrival_at)
        ? {
            rail: {
              ...(value.planned_departure_at
                ? { etd: localDateTime(value.planned_departure_at).slice(0, 10) }
                : {}),
              ...(value.planned_arrival_at
                ? { eta: localDateTime(value.planned_arrival_at).slice(0, 10) }
                : {}),
              ...(value.actual_departure_at
                ? { departure_date: localDateTime(value.actual_departure_at).slice(0, 10) }
                : {}),
              ...(value.actual_arrival_at
                ? { arrival_date: localDateTime(value.actual_arrival_at).slice(0, 10) }
                : {}),
            },
          }
        : {}),
    ...(value.international_road ? { international_road: { ...value.international_road } } : {}),
    ...(value.pickup ? { pickup: { ...value.pickup } } : {}),
    id: value.id,
    mode: value.mode,
    ...(value.origin && value.mode !== 'rail' && value.mode !== 'international_rail'
      ? { origin: { ...value.origin } }
      : {}),
    ...(value.destination && value.mode !== 'rail' && value.mode !== 'international_rail'
      ? { destination: { ...value.destination } }
      : {}),
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
        if (!point) continue;
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
