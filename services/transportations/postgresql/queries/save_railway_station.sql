INSERT INTO railway_stations (esr_code, name, country_code)
VALUES ($1, $2, $3)
ON CONFLICT (esr_code) DO UPDATE
SET name = EXCLUDED.name,
    country_code = EXCLUDED.country_code,
    archived = FALSE
WHERE $4
RETURNING ROW(esr_code, name, country_code)::railway_station_v1;
