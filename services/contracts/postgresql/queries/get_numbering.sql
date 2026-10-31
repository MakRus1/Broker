SELECT number_template, reset_number_yearly, revision
FROM contract_numbering
WHERE organization_id = $1::UUID;
