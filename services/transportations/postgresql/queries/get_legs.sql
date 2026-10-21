SELECT document
FROM transportation_legs
WHERE tenant_id = $1::UUID AND transportation_id = $2::UUID
ORDER BY position;
