-- $1 — организация.
-- $2 — поставка.
-- Последний участок с фактом определяет положение перевозки. Плановые даты не участвуют.
SELECT latest.current_segment_id, COUNT(*) AS quantity
FROM transportations AS transportation
LEFT JOIN LATERAL (
    SELECT CASE
        WHEN facts.departed AND NOT facts.arrived THEN (stage.document->>'id')::UUID
    END AS current_segment_id
    FROM jsonb_array_elements(transportation.route_segments) WITH ORDINALITY AS stage(document, position)
    CROSS JOIN LATERAL (
        SELECT
            COALESCE(stage.document->>'actual_departure_at',
                CASE WHEN stage.document->>'mode' = 'international_road'
                    THEN stage.document#>>'{international_road,departure_date}' END) IS NOT NULL AS departed,
            COALESCE(stage.document->>'actual_arrival_at',
                CASE WHEN stage.document->>'mode' = 'international_road'
                    THEN stage.document#>>'{international_road,unloading_date}' END) IS NOT NULL AS arrived
    ) AS facts
    WHERE stage.document->>'inherited' IS DISTINCT FROM 'false'
        AND (facts.departed OR facts.arrived)
    ORDER BY stage.position DESC
    LIMIT 1
) AS latest ON TRUE
WHERE transportation.organization_id = $1::UUID AND transportation.shipment_id = $2::UUID
GROUP BY latest.current_segment_id
ORDER BY latest.current_segment_id;
