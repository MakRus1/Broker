INSERT INTO access_audit (organization_id, actor, action, document)
VALUES ($1::UUID, $2, $3, $4::JSONB);
