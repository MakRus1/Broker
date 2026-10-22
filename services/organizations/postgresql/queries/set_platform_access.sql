INSERT INTO platform_access AS a (issuer, subject, permissions, revision)
SELECT $1, $2, $3::TEXT[], 1 WHERE $4::BIGINT = 0
ON CONFLICT (issuer, subject) DO UPDATE
SET permissions = EXCLUDED.permissions, revision = a.revision + 1
WHERE a.revision = $4
RETURNING ROW(permissions, revision)::public.platform_access_v1;
