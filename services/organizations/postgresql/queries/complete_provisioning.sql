UPDATE user_provisioning SET subject = $2
WHERE request_id = $1::UUID;
