ALTER TABLE shipment_route_projections
    ADD COLUMN customer_id UUID,
    ADD COLUMN customer_revision BIGINT;

CREATE INDEX shipment_route_projections_customer_idx
    ON shipment_route_projections (organization_id, customer_id, shipment_id)
    WHERE customer_id IS NOT NULL;
