UPDATE financial_labels AS stored
SET
    name = value.name,
    direction = value.direction,
    unit = value.unit,
    currency = value.currency,
    vat_mode = value.vat_mode,
    vat_rate_basis_points = value.vat_rate_basis_points,
    note = value.note,
    archived = value.archived,
    revision = value.revision
FROM (SELECT ($2::public.financial_label_v1).*) AS value
WHERE stored.organization_id = $1::UUID
    AND stored.id = value.id
    AND stored.revision = $3::BIGINT
RETURNING ROW(
    stored.id,
    stored.name,
    stored.direction,
    stored.unit,
    stored.currency,
    stored.vat_mode,
    stored.vat_rate_basis_points,
    stored.note,
    stored.archived,
    stored.revision
)::public.financial_label_v1;
