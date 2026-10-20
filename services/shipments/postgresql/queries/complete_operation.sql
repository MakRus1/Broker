UPDATE creation_operations SET status=$4,result=$5,lease_until=NULL,lease_token=NULL
WHERE tenant_id=$1::uuid AND id=$2::uuid AND lease_token=$3::uuid AND status='pending';
