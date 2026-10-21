UPDATE creation_operations
SET
    status = ($2::public.creation_operation_v1).status,
    result = ($2::public.creation_operation_v1).result,
    lease_until = NULL,
    lease_token = NULL
WHERE tenant_id = $1::UUID
  AND id = ($2::public.creation_operation_v1).id
  AND lease_token = $3::UUID
  AND status = 'pending';
