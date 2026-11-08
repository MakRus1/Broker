UPDATE sales_cards AS card
SET next_action = $3->>'next_action',
    next_action_date = ($3->>'next_action_date')::DATE,
    revision = revision + 1
FROM sales_card_action_sources AS source
WHERE card.organization_id = $1::UUID AND card.id = ($3->>'card_id')::UUID
    AND source.organization_id = card.organization_id AND source.card_id = card.id
    AND source.contact_id = $2::UUID
    AND (card.next_action IS DISTINCT FROM $3->>'next_action'
        OR card.next_action_date IS DISTINCT FROM ($3->>'next_action_date')::DATE)
RETURNING jsonb_strip_nulls(to_jsonb(card));
