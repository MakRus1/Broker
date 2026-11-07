SELECT jsonb_build_object(
    'revision', revision, 'actor', actor, 'created_at', created_at, 'snapshot', snapshot)
FROM sales_history
WHERE organization_id = $1::UUID AND entity = 'cards'
    AND id = $2::UUID AND revision > $3
ORDER BY revision
LIMIT 100;
