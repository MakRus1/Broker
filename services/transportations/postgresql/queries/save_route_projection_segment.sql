INSERT INTO shipment_route_segments (organization_id, shipment_id, id, mode, document)
VALUES ($1::UUID, $2::UUID, $3::UUID, $4::TEXT, $5::JSONB);
