CREATE TABLE shipment_customer_projections (
    organization_id UUID NOT NULL,
    shipment_id UUID NOT NULL,
    revision BIGINT NOT NULL,
    customer_id UUID NOT NULL,
    PRIMARY KEY (organization_id, shipment_id)
);

CREATE INDEX shipment_customer_projections_customer_idx
    ON shipment_customer_projections (organization_id, customer_id, shipment_id);
