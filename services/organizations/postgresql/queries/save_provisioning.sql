INSERT INTO user_provisioning (request_id, organization_id, actor, payload)
VALUES ($1::UUID, $2::UUID, $3, $4::JSONB)
ON CONFLICT (request_id) DO NOTHING;
