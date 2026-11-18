from conftest import ORGANIZATION


async def test_reference_permissions_upgrade_preserves_operational_access(
    pgsql, service_source_dir
):
    migration = (service_source_dir / 'postgresql/schemas/db_1'
                 / 'V0015__grant_reference_read_permissions.sql').read_text()
    old_roles = {
        'legacy-sales': ['sales.read', 'sales.write'],
        'legacy-finance': ['finance.read_service_charges'],
        'legacy-logistics': ['shipments.read', 'transportations.read'],
        'unrelated': ['counterparties.read'],
    }
    with pgsql['db_1'].cursor() as cursor:
        for identifier, permissions in old_roles.items():
            cursor.execute("""
                INSERT INTO organization_roles
                    (organization_id, id, name, description, permissions)
                VALUES (%s, %s, %s, '', %s)
            """, (ORGANIZATION, identifier, identifier, permissions))
        cursor.execute(migration)
        cursor.execute("""
            SELECT id, permissions, revision FROM organization_roles
            WHERE organization_id = %s AND id = ANY(%s) ORDER BY id
        """, (ORGANIZATION, list(old_roles)))
        first = cursor.fetchall()
        assert {identifier: revision for identifier, _, revision in first} == {
            'legacy-sales': 2, 'legacy-finance': 2, 'legacy-logistics': 2, 'unrelated': 1,
        }
        upgraded = {identifier: set(permissions) for identifier, permissions, _ in first}
        assert upgraded['legacy-sales'] == set(old_roles['legacy-sales']) | {
            'transport_locations.read', 'finance.read_financial_labels',
            'finance.read_calculation_settings',
        }
        assert upgraded['legacy-finance'] == set(old_roles['legacy-finance']) | {
            'finance.read_calculation_settings',
        }
        assert upgraded['legacy-logistics'] == set(old_roles['legacy-logistics']) | {
            'transport_locations.read',
        }
        assert upgraded['unrelated'] == set(old_roles['unrelated'])
        cursor.execute(migration)
        cursor.execute("""
            SELECT id, permissions, revision FROM organization_roles
            WHERE organization_id = %s AND id = ANY(%s) ORDER BY id
        """, (ORGANIZATION, list(old_roles)))
        assert cursor.fetchall() == first
