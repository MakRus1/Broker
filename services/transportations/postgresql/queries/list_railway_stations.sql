SELECT ROW(esr_code, name, country_code)::railway_station_v1
FROM railway_stations
WHERE NOT archived
  AND esr_code > $2
  AND ($1 = '' OR name ILIKE '%' || $1 || '%' OR esr_code LIKE $1 || '%')
ORDER BY esr_code
LIMIT 51;
