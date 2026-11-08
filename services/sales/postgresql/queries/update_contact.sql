UPDATE sales_contacts
SET kind = $3->>'kind',
    contact_date = ($3->>'contact_date')::DATE,
    result = $3->>'result',
    next_action = $3->>'next_action',
    next_action_date = ($3->>'next_action_date')::DATE,
    revision = revision + 1
WHERE organization_id = $1::UUID AND id = $2::UUID
RETURNING jsonb_strip_nulls(to_jsonb(sales_contacts) - 'organization_id' - 'request');
