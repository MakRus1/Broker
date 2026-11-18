from conftest import ORGANIZATION


async def test_shipment_creation_upgrade_preserves_existing_roles(pgsql, service_source_dir):
    migration = (service_source_dir / 'postgresql/schemas/db_1'
                 / 'V0016__grant_shipment_creation.sql').read_text()
    roles = {
        'editor': ['shipments.read', 'shipments.write'],
        'creator': ['shipments.read', 'shipments.write', 'shipments.create'],
        'reader': ['shipments.read'],
    }
    with pgsql['db_1'].cursor() as cursor:
        for identifier, permissions in roles.items():
            cursor.execute("""
                INSERT INTO organization_roles
                    (organization_id, id, name, description, permissions)
                VALUES (%s, %s, %s, '', %s)
            """, (ORGANIZATION, identifier, identifier, permissions))
        cursor.execute(migration)
        query = """
            SELECT id, permissions, revision FROM organization_roles
            WHERE organization_id = %s AND id = ANY(%s) ORDER BY id
        """
        cursor.execute(query, (ORGANIZATION, list(roles)))
        upgraded = cursor.fetchall()
        assert {identifier: (set(permissions), revision)
                for identifier, permissions, revision in upgraded} == {
            'editor': (set(roles['editor']) | {'shipments.create'}, 2),
            'creator': (set(roles['creator']), 1),
            'reader': (set(roles['reader']), 1),
        }
        cursor.execute(migration)
        cursor.execute(query, (ORGANIZATION, list(roles)))
        assert cursor.fetchall() == upgraded
