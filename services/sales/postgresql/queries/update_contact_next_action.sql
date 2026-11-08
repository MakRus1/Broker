UPDATE sales_cards
SET next_action = $3, next_action_date = $4::DATE, revision = revision + 1
WHERE organization_id = $1::UUID AND id = $2::UUID;
