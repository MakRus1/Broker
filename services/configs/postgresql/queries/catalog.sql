SELECT names.name, COALESCE(versions.revision, 0)::BIGINT, COALESCE(drafts.id, 0)::BIGINT
FROM (
    SELECT jsonb_object_keys(configs) AS name
    FROM config_revisions WHERE revision = (SELECT max(revision) FROM config_revisions)
    UNION
    SELECT document->>'config_name' FROM config_drafts WHERE document->>'status' = 'draft'
) AS names
LEFT JOIN config_versions AS versions ON versions.name = names.name
LEFT JOIN config_drafts AS drafts ON drafts.document->>'config_name' = names.name
    AND drafts.document->>'status' = 'draft'
ORDER BY names.name;
