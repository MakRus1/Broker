UPDATE sales_cards AS target
SET manager_subject = value.manager_subject,
    stage_id = value.stage_id,
    source = value.source,
    note = value.note,
    next_action = value.next_action,
    next_action_date = value.next_action_date,
    revision = target.revision + 1
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
WHERE target.organization_id = $1::UUID
    AND target.id = value.id
    AND target.customer_id = value.customer_id
    AND target.revision = $3
    AND target.revision < 9223372036854775807
RETURNING target.id::TEXT;
