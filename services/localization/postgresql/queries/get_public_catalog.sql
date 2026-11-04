SELECT entry.key, entry.value
FROM localization_catalogs AS catalog
CROSS JOIN LATERAL jsonb_each(catalog.messages) AS entry
WHERE catalog.locale = $1
  AND (entry.key LIKE 'login.%' OR entry.key LIKE 'keycloak.%')
  AND jsonb_typeof(entry.value) = 'string'
ORDER BY entry.key;
