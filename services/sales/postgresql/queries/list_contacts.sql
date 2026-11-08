SELECT jsonb_strip_nulls(to_jsonb(contact) - 'organization_id' - 'request')
FROM sales_contacts AS contact
WHERE organization_id = $1::UUID AND card_id = $2::UUID
    AND ($3::UUID IS NULL OR (contact_date, created_at, id) < (
        SELECT contact_date, created_at, id FROM sales_contacts
        WHERE organization_id = $1::UUID AND card_id = $2::UUID AND id = $3::UUID
    ))
ORDER BY contact_date DESC, created_at DESC, id DESC
LIMIT $4;
