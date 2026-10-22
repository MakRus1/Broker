UPDATE platform_access SET permissions = $3::TEXT[], revision = revision + 1
WHERE issuer = $1 AND subject = $2 AND revision = $4::BIGINT
RETURNING ROW(permissions, revision)::public.platform_access_v1;
