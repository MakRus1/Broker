SELECT
    ROW(
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
    )::public.financial_label_v1,
    {sort_key} AS sort_key
FROM financial_labels
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
