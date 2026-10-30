UPDATE invoices AS invoice
SET
    direction = value.direction,
    number = value.number,
    invoice_date = value.invoice_date,
    due_date = value.due_date,
    own_company_id = value.own_company_id,
    own_company_name = value.own_company_name,
    counterparty_id = value.counterparty_id,
    counterparty_name = value.counterparty_name,
    own_bank_account_id = value.own_bank_account_id,
    counterparty_bank_account_id = value.counterparty_bank_account_id,
    contract_id = value.contract_id,
    contract_number = value.contract_number,
    currency = value.currency,
    note = value.note,
    archived = value.archived,
    revision = value.revision,
    net_minor = value.net_minor,
    vat_minor = value.vat_minor,
    total_minor = value.total_minor
FROM (SELECT ($2::public.invoice_v1).*) AS value
WHERE invoice.organization_id = $1::UUID
    AND invoice.id = value.id
    AND invoice.direction = value.direction
    AND invoice.revision = $3::BIGINT
RETURNING ROW(
    invoice.id,
    invoice.direction,
    invoice.number,
    invoice.invoice_date,
    invoice.due_date,
    invoice.own_company_id,
    invoice.own_company_name,
    invoice.counterparty_id,
    invoice.counterparty_name,
    invoice.own_bank_account_id,
    invoice.counterparty_bank_account_id,
    invoice.contract_id,
    invoice.contract_number,
    invoice.currency,
    invoice.note,
    invoice.archived,
    invoice.revision,
    invoice.net_minor,
    invoice.vat_minor,
    invoice.total_minor
)::public.invoice_v1;
