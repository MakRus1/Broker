SELECT ROW(esr_code, name, country_code)::public.railway_station_v1
FROM railway_stations
WHERE NOT archived
  AND (esr_code LIKE $1 || '%' OR name ILIKE '%' || $1 || '%')
ORDER BY (esr_code = $1) DESC, (esr_code LIKE $1 || '%') DESC, name, esr_code
LIMIT 20;
