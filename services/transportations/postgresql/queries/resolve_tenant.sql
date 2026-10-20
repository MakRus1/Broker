SELECT tenant_id::text FROM tenant_memberships WHERE issuer=$1 AND subject=$2 AND ($3::text IS NULL OR tenant_id::text=$3) LIMIT 2;
