UPDATE shipments
SET revision = revision + 1
WHERE tenant_id = $1::UUID AND id = $2::UUID;
