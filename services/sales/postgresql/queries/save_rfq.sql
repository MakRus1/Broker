INSERT INTO sales_rfqs (organization_id, id, inquiry_id, revision, document)
VALUES ($1::UUID, ($2->'input'->>'id')::UUID, ($2->'input'->>'inquiry_id')::UUID,
    ($2->>'revision')::BIGINT, $2::JSONB)
ON CONFLICT (organization_id, id) DO UPDATE
SET revision = EXCLUDED.revision, document = EXCLUDED.document
WHERE sales_rfqs.revision = $3::BIGINT
    AND sales_rfqs.inquiry_id = EXCLUDED.inquiry_id
RETURNING id;
