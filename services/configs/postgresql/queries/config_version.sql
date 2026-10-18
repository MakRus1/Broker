SELECT COALESCE((SELECT revision FROM config_versions WHERE name = $1), 0)::bigint;
