SELECT COUNT(*)
FROM permission_releases
WHERE owner = $1 AND version = ANY($2::TEXT[]);
