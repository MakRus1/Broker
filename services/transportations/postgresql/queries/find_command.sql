SELECT
    payload,
    result
FROM processed_commands
WHERE tenant_id = $1::UUID AND operation_id = $2::UUID;
