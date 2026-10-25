INSERT INTO config_revisions(configs) VALUES ($1) RETURNING revision, configs, created_at;
