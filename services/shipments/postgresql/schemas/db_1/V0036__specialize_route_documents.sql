UPDATE shipments
SET route_segments = ARRAY(
    SELECT jsonb_populate_record(
        NULL::public.shipment_route_segment_v5,
        CASE
            WHEN document_path IS NULL THEN segment
            ELSE jsonb_set(segment, document_path, (
                SELECT COALESCE(jsonb_agg(
                    (document - 'type' - 'role') ||
                    CASE WHEN document_path[1] = 'sea' AND document ? 'role'
                        THEN jsonb_build_object('leg', document->'role')
                        ELSE '{}'::jsonb
                    END ORDER BY documents.position
                ), '[]'::jsonb)
                FROM jsonb_array_elements(segment #> document_path)
                    WITH ORDINALITY AS documents(document, position)
            ))
        END
    )
    FROM generate_subscripts(shipments.route_segments, 1) AS route(position)
    CROSS JOIN LATERAL (SELECT to_jsonb(shipments.route_segments[route.position]) AS segment) AS source
    CROSS JOIN LATERAL (SELECT CASE
        WHEN jsonb_typeof(segment #> '{sea,documents}') = 'array' THEN ARRAY['sea', 'documents']
        WHEN jsonb_typeof(segment #> '{air,documents}') = 'array' THEN ARRAY['air', 'documents']
    END AS document_path) AS path
    ORDER BY route.position
)
WHERE EXISTS (
    SELECT 1
    FROM unnest(shipments.route_segments) AS route
    WHERE (route.sea).documents IS NOT NULL OR (route.air).documents IS NOT NULL
);
