SELECT snapshot FROM sales_rfq_history
WHERE organization_id = $1::UUID AND id = $2::UUID AND revision = $3::BIGINT;
