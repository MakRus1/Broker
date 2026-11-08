SELECT document FROM sales_rfqs
WHERE organization_id = $1::UUID AND inquiry_id = $2::UUID
ORDER BY id LIMIT 100;
