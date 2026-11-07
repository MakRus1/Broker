INSERT INTO sales_history (organization_id, entity, id, revision, actor, snapshot)
VALUES ($1::UUID, $2, $3::UUID, $4, $5, $6::JSONB);
