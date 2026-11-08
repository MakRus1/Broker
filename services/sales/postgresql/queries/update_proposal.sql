UPDATE sales_proposals
SET document = $2::JSONB, revision = ($2->>'revision')::BIGINT, status = $2->>'status'
WHERE organization_id = $1::UUID AND id = ($2->'input'->>'id')::UUID
    AND inquiry_id = ($2->'input'->>'inquiry_id')::UUID AND revision = $3::BIGINT
RETURNING id;
