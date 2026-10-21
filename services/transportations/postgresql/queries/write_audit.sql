INSERT INTO business_audit (tenant_id, actor, action, entity_id, document)
VALUES ($1::UUID, $2, $3, $4::UUID, $5::JSONB);
