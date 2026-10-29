SELECT 1
FROM transportations
{route_join}
WHERE transportations.organization_id = candidate.organization_id
  AND ({key}) = candidate.value
  AND ({where})
