UPDATE localization_editor SET version = version + 1, draft = $2::JSONB WHERE scope = $1;
