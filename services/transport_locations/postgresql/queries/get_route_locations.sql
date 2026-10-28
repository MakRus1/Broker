SELECT ROW(id, kind, name, country_code, code)::public.transport_location_v1
FROM route_locations WHERE id = ANY($1::UUID[]) AND NOT archived;
