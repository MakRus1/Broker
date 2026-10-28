INSERT INTO railway_stations (
    id, name, country_code, esr_code, uic_code, esr_source, search_terms, manual_override
)
SELECT source.station_id, source.name, source.country_code, source.esr_code,
    source.uic_code, CASE WHEN source.esr_code IS NOT NULL THEN 'osm_hint' END,
    source.search_terms, FALSE
FROM railway_station_sources AS source
JOIN unnest($1::TEXT[]) AS imported(source_id) USING (source_id)
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name,
    country_code = EXCLUDED.country_code,
    esr_code = EXCLUDED.esr_code,
    uic_code = EXCLUDED.uic_code,
    esr_source = EXCLUDED.esr_source,
    search_terms = EXCLUDED.search_terms
WHERE NOT railway_stations.manual_override;
