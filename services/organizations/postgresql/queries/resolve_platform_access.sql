SELECT ROW(ARRAY(SELECT p.code FROM permission_catalog p
             WHERE p.scope = 'platform' AND p.code = ANY(a.permissions)), a.revision)::public.platform_access_v1
FROM platform_access a WHERE a.issuer = $1 AND a.subject = $2;
