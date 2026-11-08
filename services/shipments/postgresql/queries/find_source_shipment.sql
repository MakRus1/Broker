SELECT shipment_id::TEXT, payload FROM shipment_sources
WHERE organization_id = $1::UUID AND producer = $2 AND source_type = $3 AND source_id = $4;
