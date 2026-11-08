SELECT document FROM sales_proposals
WHERE organization_id = $1::UUID AND id = $2::UUID;
