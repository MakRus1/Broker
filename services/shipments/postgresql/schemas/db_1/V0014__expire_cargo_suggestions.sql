ALTER TABLE cargo_suggestions ADD COLUMN removed_at TIMESTAMPTZ;

-- Для ранее скрытых значений отсчитываем срок хранения с момента обновления схемы.
UPDATE cargo_suggestions SET removed_at = CURRENT_TIMESTAMP WHERE removed;

CREATE INDEX cargo_suggestions_active_expiration
    ON cargo_suggestions (used_at) WHERE NOT removed;
CREATE INDEX cargo_suggestions_removed_expiration
    ON cargo_suggestions (removed_at) WHERE removed;
