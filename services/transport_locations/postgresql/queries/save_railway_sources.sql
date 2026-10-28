INSERT INTO railway_station_sources (
    source_id, station_id, name, country_code, esr_code, uic_code, search_terms
)
SELECT source_id, station_id, name, country_code, esr_code, uic_code, search_terms
FROM unnest($1::public.railway_station_source_v1[])
ON CONFLICT (source_id) DO UPDATE SET
    name = EXCLUDED.name,
    country_code = EXCLUDED.country_code,
    esr_code = EXCLUDED.esr_code,
    uic_code = EXCLUDED.uic_code,
    search_terms = EXCLUDED.search_terms;
