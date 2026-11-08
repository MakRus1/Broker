INSERT INTO sales_cards (
    organization_id,
    id,
    customer_id,
    manager_subject,
    stage_id,
    source,
    note,
    next_action,
    next_action_date
)
SELECT
    $1::UUID,
    value.id,
    value.customer_id,
    value.manager_subject,
    value.stage_id,
    value.source,
    value.note,
    value.next_action,
    value.next_action_date
FROM jsonb_to_record($2::JSONB) AS value (
    id UUID,
    customer_id UUID,
    manager_subject TEXT,
    stage_id UUID,
    source TEXT,
    note TEXT,
    next_action TEXT,
    next_action_date DATE
)
ON CONFLICT DO NOTHING
RETURNING id::TEXT;
