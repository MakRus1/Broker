INSERT INTO platform_access_audit(actor, issuer, subject, permissions, revision)
VALUES ($1, $2, $3, $4::TEXT[], $5);
