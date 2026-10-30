-- source_group: 0 — услуги текущей поставки или перевозки,
-- 1 — услуги перевозок поставки, 2 — общий итог без двойного учёта.
WITH amounts AS (
    SELECT
        transportation_id IS NOT NULL AS transportation,
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
    CASE
        WHEN GROUPING(transportation) = 1 THEN 2
        WHEN $3::UUID IS NOT NULL THEN 0
        WHEN transportation THEN 1
        ELSE 0
    END::INTEGER AS source_group,
    currency,
    COALESCE(SUM(total_minor) FILTER (WHERE direction = 'income'), 0)::TEXT AS income,
    COALESCE(SUM(total_minor) FILTER (WHERE direction = 'expense'), 0)::TEXT AS expense
FROM amounts
GROUP BY GROUPING SETS ((currency, transportation), (currency))
ORDER BY currency, source_group;
