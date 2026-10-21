SELECT
    transportation_id::TEXT,
    document
FROM transportation_legs
WHERE tenant_id = $1::UUID AND transportation_id = ANY($2::UUID[])
ORDER BY transportation_id, position;
