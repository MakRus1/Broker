SELECT organization_id::TEXT, actor, payload, subject
FROM user_provisioning
WHERE request_id = $1::UUID
FOR UPDATE;
