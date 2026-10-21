CREATE TABLE shipment_items (
    tenant_id UUID NOT NULL,
    id UUID NOT NULL DEFAULT gen_random_uuid(),
    shipment_id UUID NOT NULL,
    revision BIGINT NOT NULL DEFAULT 1 CHECK (revision > 0),
    container_type_code TEXT NOT NULL CHECK (container_type_code ~ '^[A-Z0-9]{2,16}$'),
    container_ownership TEXT NOT NULL CHECK (container_ownership IN ('SOC', 'COC')),
    planned_quantity INTEGER NOT NULL CHECK (planned_quantity BETWEEN 1 AND 10000),
    PRIMARY KEY (tenant_id, id),
    FOREIGN KEY (tenant_id, shipment_id) REFERENCES shipments (tenant_id, id)
);

CREATE INDEX shipment_items_shipment_idx ON shipment_items (tenant_id, shipment_id, id);
