WITH amounts AS (
    SELECT
        $3::UUID IS NOT NULL OR transportation_id IS NULL AS direct,
        currency,
        direction,
        total_minor
    FROM service_charges
    WHERE organization_id = $1::UUID
        AND shipment_id = $2::UUID
        AND ($3::UUID IS NULL OR transportation_id = $3::UUID)
        AND NOT archived
)
SELECT
    currency,
    CASE WHEN BOOL_OR(direct) THEN
        COALESCE(SUM(total_minor) FILTER (WHERE direct AND direction = 'income'), 0)::TEXT
    END AS direct_income,
    CASE WHEN BOOL_OR(direct) THEN
        COALESCE(SUM(total_minor) FILTER (WHERE direct AND direction = 'expense'), 0)::TEXT
    END AS direct_expense,
    CASE WHEN BOOL_OR(NOT direct) THEN
        COALESCE(SUM(total_minor) FILTER (WHERE NOT direct AND direction = 'income'), 0)::TEXT
    END AS transportation_income,
    CASE WHEN BOOL_OR(NOT direct) THEN
        COALESCE(SUM(total_minor) FILTER (WHERE NOT direct AND direction = 'expense'), 0)::TEXT
    END AS transportation_expense,
    COALESCE(SUM(total_minor) FILTER (WHERE direction = 'income'), 0)::TEXT AS income,
    COALESCE(SUM(total_minor) FILTER (WHERE direction = 'expense'), 0)::TEXT AS expense
FROM amounts
GROUP BY currency
ORDER BY currency;
