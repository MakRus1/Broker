INSERT INTO financial_labels (
    organization_id,
    id,
    name,
    direction,
    unit,
    currency,
    vat_mode,
    vat_rate_basis_points,
    note,
    archived,
    revision
)
SELECT
    $1::UUID,
    value.*
FROM (SELECT ($2::public.financial_label_v1).*) AS value
ON CONFLICT (organization_id, id) DO NOTHING
RETURNING ROW(
    id,
    name,
    direction,
    unit,
    currency,
    vat_mode,
    vat_rate_basis_points,
    note,
    archived,
    revision
)::public.financial_label_v1;
