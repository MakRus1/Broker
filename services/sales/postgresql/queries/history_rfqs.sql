SELECT jsonb_build_object('snapshot', snapshot, 'actor', actor, 'created_at', created_at)
FROM sales_rfq_history
WHERE organization_id = $1::UUID AND id = $2::UUID AND revision > $3::BIGINT
ORDER BY revision LIMIT 100;
