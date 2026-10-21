SELECT ROW(
    payload,
    result
)::public.saved_command_v1
FROM processed_commands
WHERE tenant_id = $1::UUID AND operation_id = $2::UUID;
