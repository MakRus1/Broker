CREATE TYPE container_fact_v1 AS (
    container_type TEXT,
    ownership TEXT,
    operation_id UUID,
    quantity BIGINT
);
CREATE INDEX transportations_container_fact_idx
    ON transportations (organization_id, shipment_id, container_type_code, container_ownership)
    INCLUDE (type, operation_id);
