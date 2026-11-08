INSERT INTO sales_contacts (
    organization_id, id, card_id, kind, contact_date, result,
    author_subject, author_issuer, next_action, next_action_date, request
)
VALUES (
    $1::UUID, ($2->>'id')::UUID, ($2->>'card_id')::UUID, $2->>'kind',
    ($2->>'contact_date')::DATE, $2->>'result', $3, $4,
    $2->>'next_action', ($2->>'next_action_date')::DATE, $2
)
RETURNING jsonb_strip_nulls(to_jsonb(sales_contacts) - 'organization_id' - 'request');
