UPDATE organization_roles
SET permissions = array_replace(
    array_replace(permissions, 'finance.read_entries', 'finance.read_service_charges'),
    'finance.write_entries', 'finance.write_service_charges'
), revision = revision + 1
WHERE permissions && ARRAY['finance.read_entries', 'finance.write_entries'];
