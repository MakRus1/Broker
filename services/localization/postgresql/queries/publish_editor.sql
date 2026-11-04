UPDATE localization_editor
SET version = version + 1, published_revision = published_revision + 1, draft = NULL
WHERE scope = $1;
