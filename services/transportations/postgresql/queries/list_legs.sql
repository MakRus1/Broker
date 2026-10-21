SELECT ROW(
    transportation_id,
    document
)::public.transportation_leg_v1
FROM transportation_legs
WHERE tenant_id = $1::UUID AND transportation_id = ANY($2::UUID[])
ORDER BY transportation_id, position;
