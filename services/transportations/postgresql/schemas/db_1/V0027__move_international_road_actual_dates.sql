-- Переносим даты на уровень участка и сохраняем исходную точность до дня.
WITH migrated AS (
    SELECT transportation.organization_id, transportation.id, jsonb_agg(
        CASE WHEN segment->>'mode' = 'international_road'
                 AND (segment->'international_road') ?| ARRAY['departure_date', 'unloading_date']
            THEN (segment #- '{international_road,departure_date}' #- '{international_road,unloading_date}')
                || jsonb_strip_nulls(jsonb_build_object(
                    'actual_departure_date', COALESCE(segment->>'actual_departure_date',
                        segment#>>'{international_road,departure_date}'),
                    'actual_arrival_date', COALESCE(segment->>'actual_arrival_date',
                        segment#>>'{international_road,unloading_date}')))
            ELSE segment
        END ORDER BY position) AS route_segments
    FROM transportations AS transportation
    CROSS JOIN LATERAL jsonb_array_elements(transportation.route_segments)
        WITH ORDINALITY AS route(segment, position)
    GROUP BY transportation.organization_id, transportation.id
)
UPDATE transportations AS transportation
SET route_segments = migrated.route_segments
FROM migrated
WHERE transportation.organization_id = migrated.organization_id
    AND transportation.id = migrated.id
    AND transportation.route_segments IS DISTINCT FROM migrated.route_segments;
