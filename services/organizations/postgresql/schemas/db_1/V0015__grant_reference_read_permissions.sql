-- Отделяем справочники и параметры расчёта от просмотра фактических операций.
WITH required AS (
    SELECT organization_id, id,
        CASE WHEN permissions && ARRAY['sales.read', 'sales.write', 'shipments.read', 'transportations.read']
            THEN ARRAY['transport_locations.read'] ELSE ARRAY[]::TEXT[] END
        || CASE WHEN permissions && ARRAY['sales.write', 'finance.read_service_charges']
            THEN ARRAY['finance.read_calculation_settings'] ELSE ARRAY[]::TEXT[] END
        || CASE WHEN permissions @> ARRAY['sales.write']
            THEN ARRAY['finance.read_financial_labels'] ELSE ARRAY[]::TEXT[] END AS additions
    FROM organization_roles
)
UPDATE organization_roles AS role
SET permissions = ARRAY(
        SELECT DISTINCT permission
        FROM unnest(role.permissions || required.additions) AS permission
        ORDER BY permission
    ),
    revision = role.revision + 1
FROM required
WHERE role.organization_id = required.organization_id AND role.id = required.id
    AND NOT role.permissions @> required.additions;
