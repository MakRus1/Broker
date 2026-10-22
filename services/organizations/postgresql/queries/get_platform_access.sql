SELECT ROW(permissions, revision)::public.platform_access_v1 FROM platform_access WHERE issuer = $1 AND subject = $2;
