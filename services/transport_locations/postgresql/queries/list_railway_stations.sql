SELECT
    ROW(
        esr_code,
        name,
        country_code
    )::public.railway_station_v1,
    {sort_key} AS sort_key
FROM railway_stations
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
