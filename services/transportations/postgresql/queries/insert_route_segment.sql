INSERT INTO route_segments (organization_id, transportation_id, id, position, document)
VALUES ($1::UUID, $2::UUID, $3::UUID, $4, $5::JSONB);
