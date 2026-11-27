CREATE TABLE voyages (
    organization_id UUID NOT NULL,
    id UUID NOT NULL DEFAULT gen_random_uuid(),
    number TEXT NOT NULL CHECK (btrim(number) <> ''),
    PRIMARY KEY (organization_id, id)
);

CREATE TABLE voyage_port_calls (
    organization_id UUID NOT NULL,
    voyage_id UUID NOT NULL,
    position INTEGER NOT NULL CHECK (position > 0),
    port public.transport_location_v1 NOT NULL,
    CHECK ((port).kind IS NOT DISTINCT FROM 'sea_port' AND (port).id IS NOT NULL AND (port).name IS NOT NULL),
    PRIMARY KEY (organization_id, voyage_id, position),
    FOREIGN KEY (organization_id, voyage_id) REFERENCES voyages ON DELETE CASCADE
);

CREATE TABLE voyage_assignments (
    organization_id UUID NOT NULL,
    shipment_id UUID NOT NULL,
    segment_id UUID NOT NULL,
    voyage_id UUID NOT NULL,
    loading_call INTEGER NOT NULL,
    discharge_call INTEGER NOT NULL CHECK (discharge_call > loading_call),
    PRIMARY KEY (organization_id, shipment_id, segment_id),
    FOREIGN KEY (organization_id, shipment_id) REFERENCES shipments ON DELETE CASCADE,
    FOREIGN KEY (organization_id, voyage_id, loading_call) REFERENCES voyage_port_calls,
    FOREIGN KEY (organization_id, voyage_id, discharge_call) REFERENCES voyage_port_calls
);

CREATE INDEX voyage_assignments_by_voyage ON voyage_assignments (organization_id, voyage_id);

CREATE VIEW confirmed_voyage_bookings AS
SELECT a.organization_id, a.voyage_id, a.shipment_id, a.segment_id, s.number AS shipment_number,
       ((leg.sea).booking).number AS booking_number,
       (loading.port).name AS load_port, (discharge.port).name AS discharge_port
FROM voyage_assignments a
JOIN shipments s ON (s.organization_id, s.id) = (a.organization_id, a.shipment_id)
JOIN voyage_port_calls loading ON (loading.organization_id, loading.voyage_id, loading.position)
    = (a.organization_id, a.voyage_id, a.loading_call)
JOIN voyage_port_calls discharge ON (discharge.organization_id, discharge.voyage_id, discharge.position)
    = (a.organization_id, a.voyage_id, a.discharge_call)
CROSS JOIN LATERAL unnest(s.route_segments) leg
WHERE leg.id = a.segment_id AND leg.type = 'sea' AND ((leg.sea).booking).status = 'confirmed';
