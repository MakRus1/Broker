SELECT ROW(
    transportation_id,
    document
)::public.route_segment_v1
FROM route_segments
WHERE organization_id = $1::UUID AND transportation_id = ANY($2::UUID[])
ORDER BY transportation_id, position;
