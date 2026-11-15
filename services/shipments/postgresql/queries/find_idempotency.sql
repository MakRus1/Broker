-- $1 — организация.
-- $2 — действие, в пределах которого действует ключ.
-- $3 — ключ идемпотентности.
SELECT ROW(
    request,
    result
)::public.idempotent_result_v1
FROM idempotent_results
WHERE organization_id = $1::UUID AND action = $2 AND key = $3;
