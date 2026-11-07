UPDATE sales_inquiries
SET conversion = $3::JSONB
WHERE organization_id = $1::UUID AND id = $2::UUID AND conversion IS NULL;
