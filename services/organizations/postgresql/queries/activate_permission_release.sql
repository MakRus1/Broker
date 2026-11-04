UPDATE permission_releases
SET active = TRUE
WHERE owner = $1 AND version = $2;
