SELECT ROW(id, name, country_code, esr_code, uic_code, esr_source)::public.railway_station_v2
FROM railway_stations
WHERE NOT archived AND id = ANY($1::uuid[]);
