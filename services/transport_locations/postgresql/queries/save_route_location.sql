INSERT INTO route_locations (id, kind, name, country_code, code, manual_override)
VALUES ($1::UUID, $2, $3, $4, $5, TRUE)
ON CONFLICT (id) DO UPDATE
SET name = EXCLUDED.name, country_code = EXCLUDED.country_code,
    code = EXCLUDED.code, archived = FALSE, manual_override = TRUE
WHERE $6 AND route_locations.kind = EXCLUDED.kind
RETURNING ROW(id, kind, name, country_code, code)::public.transport_location_v1;
