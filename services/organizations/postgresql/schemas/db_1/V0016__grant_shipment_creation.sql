-- Сохраняем возможность создания у ролей, которым ранее было разрешено изменение поставок.
UPDATE organization_roles
SET permissions = array_append(permissions, 'shipments.create'), revision = revision + 1
WHERE permissions @> ARRAY['shipments.write']
    AND NOT permissions @> ARRAY['shipments.create'];
