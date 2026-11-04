UPDATE permission_releases
SET active = (version = ANY($2::TEXT[]))
WHERE owner = $1;
