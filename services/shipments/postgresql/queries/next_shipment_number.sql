INSERT INTO shipment_number_counters AS c (organization_id, period, value)
VALUES ($1::UUID, $2, 1)
ON CONFLICT (organization_id, period) DO UPDATE SET value = c.value + 1
RETURNING value;
