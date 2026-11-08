WITH updated AS (
    UPDATE sales_cards
    SET next_action = $3, next_action_date = $4::DATE, revision = revision + 1
    WHERE organization_id = $1::UUID AND id = $2::UUID
    RETURNING organization_id, id
)
INSERT INTO sales_card_action_sources (organization_id, card_id, contact_id)
SELECT organization_id, id, $5::UUID FROM updated
ON CONFLICT (organization_id, card_id)
DO UPDATE SET contact_id = EXCLUDED.contact_id;
