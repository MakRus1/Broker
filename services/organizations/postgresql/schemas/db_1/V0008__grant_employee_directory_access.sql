-- Существующие роли сохраняют возможность читать поставки после выделения зависимости.
UPDATE organization_roles
SET permissions = array_append(permissions, 'organizations.read_employees')
WHERE ('shipments.read' = ANY(permissions) OR 'organizations.manage' = ANY(permissions))
    AND NOT 'organizations.read_employees' = ANY(permissions);
