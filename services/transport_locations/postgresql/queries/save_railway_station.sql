INSERT INTO railway_stations (id, name, country_code, esr_code, uic_code, esr_source, manual_override)
VALUES ($1::uuid, $2, $3, $4, $5, CASE WHEN $4::text IS NOT NULL THEN 'manual' END, TRUE)
ON CONFLICT (id) DO UPDATE
SET name = EXCLUDED.name,
    country_code = EXCLUDED.country_code,
    esr_code = EXCLUDED.esr_code,
    uic_code = EXCLUDED.uic_code,
    esr_source = EXCLUDED.esr_source,
    manual_override = TRUE,
    archived = FALSE
WHERE $6
RETURNING ROW(id, name, country_code, esr_code, uic_code, esr_source)::public.railway_station_v2;
