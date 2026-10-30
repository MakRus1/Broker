INSERT INTO financial_entry_history (
    organization_id, entry_id, revision, actor, snapshot
)
VALUES ($1::UUID, $2::UUID, $3::BIGINT, $4::TEXT, $5::JSONB);
