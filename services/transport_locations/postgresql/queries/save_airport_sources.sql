-- Стабильный ID поставщика сохраняет UUID даже при смене названия или кода.
-- Имеющиеся ручные аэропорты связываются только по точному коду и стране.
INSERT INTO airport_sources (
    source_id, location_id, name, country_code, iata_code, icao_code, search_terms
)
SELECT
    airport.source_id,
    COALESCE(existing.location_id, matching.id, airport.location_id),
    airport.name,
    airport.country_code,
    airport.iata_code,
    airport.icao_code,
    airport.search_terms
FROM unnest($1::public.airport_source_v1[]) AS airport
LEFT JOIN airport_sources AS existing USING (source_id)
LEFT JOIN LATERAL (
    SELECT (array_agg(candidate.id))[1] AS id
    FROM route_locations AS candidate
    WHERE candidate.kind = 'airport'
        AND candidate.country_code = airport.country_code
        AND candidate.code IN (airport.iata_code, airport.icao_code)
    HAVING count(*) = 1
) AS matching ON existing.source_id IS NULL
    AND NOT EXISTS (
        SELECT 1 FROM airport_sources AS linked WHERE linked.location_id = matching.id
    )
ON CONFLICT (source_id) DO UPDATE SET
    name = EXCLUDED.name,
    country_code = EXCLUDED.country_code,
    iata_code = EXCLUDED.iata_code,
    icao_code = EXCLUDED.icao_code,
    search_terms = EXCLUDED.search_terms;
