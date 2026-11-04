-- scope = global для общих переводов, UUID организации для её переопределений.
CREATE TABLE localization_editor (
    scope TEXT PRIMARY KEY,
    version BIGINT NOT NULL DEFAULT 0 CHECK (version >= 0),
    published_revision BIGINT NOT NULL DEFAULT 0 CHECK (published_revision >= 0),
    draft JSONB,
    CHECK (draft IS NULL OR jsonb_typeof(draft) = 'object')
);
INSERT INTO localization_editor (scope, published_revision)
SELECT 'global', MAX(revision) FROM localization_catalogs HAVING COUNT(*) > 0;
INSERT INTO localization_editor (scope, published_revision)
SELECT organization_id::TEXT, MAX(revision)
FROM organization_localization_catalogs
GROUP BY organization_id;
