SELECT ROW(subject, display_name)::public.organization_employee_v1
FROM memberships
WHERE organization_id = $1::UUID AND enabled
    AND ($2::TEXT IS NULL OR subject = $2)
    AND ($3::TEXT IS NULL OR display_name ILIKE '%' || $3 || '%')
ORDER BY display_name, subject
LIMIT $4;
