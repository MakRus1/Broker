-- Пятисимвольный LOCODE — устойчивый ключ источника.
-- Ручные трёхсимвольные коды связываются только вместе со страной.
INSERT INTO sea_port_sources (locode, location_id, name, country_code, search_terms)
SELECT
    port.locode,
    COALESCE(existing.location_id, matching.id, port.location_id),
    port.name,
    port.country_code,
    port.search_terms
FROM unnest($1::public.sea_port_source_v1[]) AS port
LEFT JOIN sea_port_sources AS existing USING (locode)
LEFT JOIN LATERAL (
    SELECT (array_agg(candidate.id))[1] AS id
    FROM route_locations AS candidate
    WHERE candidate.kind = 'sea_port'
        AND candidate.country_code = port.country_code
        AND candidate.code IN (port.locode, substring(port.locode FROM 3))
    HAVING count(*) = 1
) AS matching ON existing.locode IS NULL
    AND NOT EXISTS (
        SELECT 1 FROM sea_port_sources AS linked WHERE linked.location_id = matching.id
    )
ON CONFLICT (locode) DO UPDATE SET
    name = EXCLUDED.name,
    country_code = EXCLUDED.country_code,
    search_terms = EXCLUDED.search_terms;
