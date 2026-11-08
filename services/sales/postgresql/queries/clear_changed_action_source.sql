DELETE FROM sales_card_action_sources AS source
USING sales_cards AS card
WHERE source.organization_id = $1::UUID AND source.card_id = ($2->>'id')::UUID
    AND card.organization_id = source.organization_id AND card.id = source.card_id
    AND (card.next_action IS DISTINCT FROM $2->>'next_action'
        OR card.next_action_date IS DISTINCT FROM ($2->>'next_action_date')::DATE);
