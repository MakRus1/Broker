UPDATE sales_inquiries
SET accepted_proposal_id = $3::UUID,
    accepted_proposal_revision = $4::BIGINT,
    status = 'agreed',
    revision = revision + 1
WHERE organization_id = $1::UUID AND id = $2::UUID;
