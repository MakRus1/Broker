SELECT ROW(id, kind, name, country_code, code)::public.transport_location_v1
FROM route_locations
WHERE kind = $1 AND NOT archived
    AND (
        $2 = ''
        OR (lower(name || ' ' || COALESCE(code, '') || ' ' || COALESCE(search_terms, ''))
            COLLATE "C") LIKE '%' || lower($2) || '%'
    )
ORDER BY (code = $2) DESC NULLS LAST, name, id
LIMIT 20;
