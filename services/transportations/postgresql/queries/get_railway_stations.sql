SELECT ROW(esr_code, name, country_code)::public.railway_station_v1
FROM railway_stations
WHERE NOT archived AND esr_code = ANY($1::TEXT[]);
