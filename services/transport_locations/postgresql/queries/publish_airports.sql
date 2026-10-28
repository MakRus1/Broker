INSERT INTO route_locations (id, kind, name, country_code, code, search_terms)
SELECT
    source.location_id,
    'airport',
    source.name,
    source.country_code,
    COALESCE(source.iata_code, source.icao_code),
    source.search_terms
FROM airport_sources AS source
JOIN unnest($1::BIGINT[]) AS imported(source_id) USING (source_id)
ON CONFLICT (id) DO UPDATE SET
    name = CASE WHEN route_locations.manual_override
        THEN route_locations.name ELSE EXCLUDED.name END,
    country_code = CASE WHEN route_locations.manual_override
        THEN route_locations.country_code ELSE EXCLUDED.country_code END,
    code = CASE WHEN route_locations.manual_override
        THEN route_locations.code ELSE EXCLUDED.code END,
    search_terms = EXCLUDED.search_terms;
