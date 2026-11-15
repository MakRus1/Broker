-- $1 — организация.
-- $2 — результат операции создания перевозок с её идентификатором и статусом.
UPDATE creation_operations
SET
    status = ($2::public.creation_operation_v1).status,
    result = ($2::public.creation_operation_v1).result
WHERE organization_id = $1::UUID
  AND id = ($2::public.creation_operation_v1).id
  AND status = 'pending';
