CREATE TABLE shipment_sources (
    organization_id UUID NOT NULL,
    producer TEXT NOT NULL,
    source_type TEXT NOT NULL,
    source_id TEXT NOT NULL,
    shipment_id UUID NOT NULL,
    payload JSONB NOT NULL,
    PRIMARY KEY (organization_id, producer, source_type, source_id),
    UNIQUE (organization_id, shipment_id),
    FOREIGN KEY (organization_id, shipment_id) REFERENCES shipments (organization_id, id)
);
