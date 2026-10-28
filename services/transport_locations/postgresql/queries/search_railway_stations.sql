SELECT ROW(id, name, country_code, esr_code, uic_code, esr_source)::public.railway_station_v2
FROM railway_stations
WHERE NOT archived AND lower(name || ' ' || coalesce(esr_code, '') || ' ' || coalesce(uic_code, '') || ' ' || search_terms) COLLATE "C" LIKE '%' || lower($1) || '%'
ORDER BY (coalesce(esr_code = $1, FALSE) OR coalesce(uic_code = $1, FALSE)) DESC, name, id
LIMIT 20;
