CREATE TABLE transportations (
    tenant_id UUID NOT NULL,
    id UUID NOT NULL DEFAULT gen_random_uuid(),
    shipment_id UUID NOT NULL,
    item_id UUID NOT NULL,
    operation_id UUID NOT NULL,
    ordinal INTEGER NOT NULL,
    revision BIGINT NOT NULL DEFAULT 1 CHECK (revision > 0),
    container_type_code TEXT NOT NULL,
    container_ownership TEXT NOT NULL CHECK (container_ownership IN ('SOC', 'COC')),
    PRIMARY KEY (tenant_id, id),
    UNIQUE (tenant_id, operation_id, ordinal),
    FOREIGN KEY (tenant_id, operation_id) REFERENCES processed_commands (tenant_id, operation_id)
);

CREATE INDEX transportations_shipment_idx ON transportations (tenant_id, shipment_id, id);
CREATE INDEX transportations_item_idx ON transportations (tenant_id, item_id, id);
