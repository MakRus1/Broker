INSERT INTO document_number_counters AS counter (organization_id, kind, period, value)
VALUES (
    $1::UUID,
    $2::TEXT,
    CASE WHEN $3::BOOLEAN THEN EXTRACT(YEAR FROM CURRENT_DATE)::INTEGER ELSE 0 END,
    1
)
ON CONFLICT (organization_id, kind, period) DO UPDATE SET value = counter.value + 1
RETURNING CURRENT_DATE, value;
