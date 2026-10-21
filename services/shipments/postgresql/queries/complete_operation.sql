UPDATE creation_operations
SET
    status = $4,
    result = $5::JSONB,
    lease_until = NULL,
    lease_token = NULL
WHERE tenant_id = $1::UUID
  AND id = $2::UUID
  AND lease_token = $3::UUID
  AND status = 'pending';
