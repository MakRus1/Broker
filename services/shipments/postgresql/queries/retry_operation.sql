UPDATE creation_operations SET next_attempt_at=now()+make_interval(secs=>least(60,attempts*2)),lease_until=NULL,lease_token=NULL
WHERE tenant_id=$1::uuid AND id=$2::uuid AND lease_token=$3::uuid AND status='pending';
