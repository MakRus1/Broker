-- $1 — идентификатор запроса создания пользователя.
SELECT organization_id, actor, payload, subject
FROM user_provisioning
WHERE request_id = $1::UUID
FOR UPDATE;
