SELECT ROW(
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
)::public.financial_label_v1
FROM financial_labels
WHERE organization_id = $1::UUID AND id = $2::UUID;
