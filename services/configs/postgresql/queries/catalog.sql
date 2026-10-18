SELECT names.name, COALESCE(versions.revision, 0)::bigint, COALESCE(drafts.id, 0)::bigint
FROM jsonb_object_keys($1::jsonb) AS names(name)
LEFT JOIN config_versions AS versions ON versions.name = names.name
LEFT JOIN config_drafts AS drafts ON drafts.document->>'config_name' = names.name
    AND drafts.document->>'status' = 'draft'
ORDER BY names.name;
