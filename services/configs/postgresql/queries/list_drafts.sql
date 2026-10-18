SELECT id, document FROM config_drafts WHERE ($1 = '' OR document->>'config_name' = $1)
AND ($2::bigint = 0 OR id < $2) ORDER BY id DESC LIMIT 100;
