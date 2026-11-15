-- $1 — организация.
-- $2 — идентификатор карточки работы с клиентом.
-- $3 — идентификатор контакта, раньше которого читать список (NULL для первой страницы).
-- $4 — лимит строк с учётом дополнительной строки для проверки следующей страницы.
SELECT jsonb_strip_nulls(to_jsonb(contact) - 'organization_id' - 'request')
FROM sales_contacts AS contact
WHERE organization_id = $1::UUID AND card_id = $2::UUID
    AND ($3::UUID IS NULL OR (contact_date, created_at, id) < (
        SELECT contact_date, created_at, id FROM sales_contacts
        WHERE organization_id = $1::UUID AND card_id = $2::UUID AND id = $3::UUID
    ))
ORDER BY contact_date DESC, created_at DESC, id DESC
LIMIT $4;
