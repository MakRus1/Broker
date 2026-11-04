INSERT INTO localization_editor (scope) VALUES ($1) ON CONFLICT DO NOTHING;
