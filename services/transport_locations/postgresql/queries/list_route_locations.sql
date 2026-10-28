SELECT
    ROW(
        id,
        kind,
        name,
        country_code,
        code
    )::public.transport_location_v1,
    {sort_key} AS sort_key
FROM route_locations
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
