SELECT ROW(
    request,
    result
)::public.idempotent_result_v1
FROM idempotent_results
WHERE tenant_id = $1::UUID AND action = $2 AND key = $3;
