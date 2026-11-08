SELECT conversion FROM sales_inquiries
WHERE organization_id = $1::UUID AND id = $2::UUID;
