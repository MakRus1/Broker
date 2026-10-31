SELECT id
FROM payments
WHERE organization_id = $1::UUID AND id = $2::UUID
FOR UPDATE;
