SELECT jsonb_build_object(
    'ru', COALESCE(jsonb_object_agg(key, ru), '{}'::JSONB),
    'en', COALESCE(jsonb_object_agg(key, en), '{}'::JSONB)
)
FROM localization_source;
