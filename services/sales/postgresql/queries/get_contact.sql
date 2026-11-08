SELECT jsonb_strip_nulls(to_jsonb(contact) - 'organization_id' - 'request'), request
FROM sales_contacts AS contact
WHERE organization_id = $1::UUID AND id = $2::UUID;
