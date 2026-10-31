INSERT INTO document_number_counters AS counter (organization_id, kind, period, value)
VALUES ($1::UUID, $2::TEXT, $3::INTEGER, 1)
ON CONFLICT (organization_id, kind, period) DO UPDATE SET value = counter.value + 1
RETURNING value;
