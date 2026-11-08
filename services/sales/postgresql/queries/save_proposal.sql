INSERT INTO sales_proposals (organization_id, inquiry_id, id, revision, status, document)
SELECT $1::UUID, ($2->'input'->>'inquiry_id')::UUID, ($2->'input'->>'id')::UUID,
    ($2->>'revision')::BIGINT, $2->>'status', $2::JSONB
WHERE $3::BIGINT = 0
ON CONFLICT (organization_id, id) DO NOTHING
RETURNING id;
