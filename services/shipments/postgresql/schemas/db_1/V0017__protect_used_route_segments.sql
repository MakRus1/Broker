CREATE TABLE shipment_route_segment_usage (
    organization_id UUID NOT NULL,
    shipment_id UUID NOT NULL,
    segment_id UUID NOT NULL,
    PRIMARY KEY (organization_id, shipment_id, segment_id),
    FOREIGN KEY (organization_id, shipment_id) REFERENCES shipments (organization_id, id)
);
