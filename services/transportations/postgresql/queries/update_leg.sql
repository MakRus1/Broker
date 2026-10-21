UPDATE transportation_legs
SET document = $4::JSONB
WHERE tenant_id = $1::UUID AND transportation_id = $2::UUID AND id = $3::UUID;
