SELECT
    ROW(id, name, country_code, esr_code, uic_code, esr_source)::public.railway_station_v2,
    {sort_key} AS sort_key
FROM railway_stations
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
