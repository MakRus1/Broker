-- Чтение перед удалением/сменой типа участка инвалидирует незавершённые записи маршрута.
CREATE TABLE route_usage_checks (
    organization_id UUID NOT NULL,
    shipment_id UUID NOT NULL,
    version BIGINT NOT NULL,
    PRIMARY KEY (organization_id, shipment_id)
);
