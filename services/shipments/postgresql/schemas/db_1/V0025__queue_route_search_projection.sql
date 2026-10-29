-- Текущее состояние по поставке; новые версии заменяют ещё не доставленные старые.
CREATE TABLE route_projection_outbox (
    organization_id UUID NOT NULL,
    shipment_id UUID NOT NULL,
    revision BIGINT NOT NULL,
    route_segments JSONB NOT NULL,
    next_attempt_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    lease_until TIMESTAMPTZ,
    lease_token UUID,
    PRIMARY KEY (organization_id, shipment_id)
);

CREATE INDEX route_projection_due_idx ON route_projection_outbox (next_attempt_at, shipment_id);

-- Начальное заполнение существующих поставок; дальнейшие изменения пишет сервис.
INSERT INTO route_projection_outbox (organization_id, shipment_id, revision, route_segments)
SELECT organization_id, id, revision, to_jsonb(route_segments)
FROM shipments;
