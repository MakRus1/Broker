ALTER TABLE service_charges ALTER COLUMN shipment_id DROP NOT NULL;
ALTER TABLE service_charges ADD CONSTRAINT transportation_requires_shipment
    CHECK (transportation_id IS NULL OR shipment_id IS NOT NULL);
