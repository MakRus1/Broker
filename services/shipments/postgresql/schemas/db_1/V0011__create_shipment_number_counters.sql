CREATE TABLE shipment_number_counters (
    organization_id UUID NOT NULL,
    period INTEGER NOT NULL,
    value BIGINT NOT NULL CHECK (value > 0),
    PRIMARY KEY (organization_id, period)
);
-- Продолжаем прежнюю нумерацию при переходе на счётчик организации.
INSERT INTO shipment_number_counters (organization_id, period, value)
SELECT
    organization_id,
    0,
    MAX(substring(number FROM '^S-([0-9]+)$')::BIGINT)
FROM shipments
GROUP BY organization_id
HAVING MAX(substring(number FROM '^S-([0-9]+)$')::BIGINT) IS NOT NULL;
