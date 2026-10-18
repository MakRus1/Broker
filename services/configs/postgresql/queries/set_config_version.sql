INSERT INTO config_versions(name, revision) VALUES ($1, $2)
ON CONFLICT (name) DO UPDATE SET revision = EXCLUDED.revision;
