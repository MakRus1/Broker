UPDATE sales_inquiries
SET shipment_id = $3::UUID, revision = revision + 1
WHERE organization_id = $1::UUID AND id = $2::UUID AND shipment_id IS NULL
RETURNING id::TEXT;
