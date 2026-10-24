SELECT ROW(id, kind, name, country_code, code)::public.transport_location_v1
FROM route_locations
WHERE kind = $1 AND NOT archived AND ($3::UUID IS NULL OR id > $3::UUID)
  AND ($2 = '' OR name ILIKE '%' || $2 || '%' OR code ILIKE $2 || '%')
ORDER BY id
LIMIT 51;
