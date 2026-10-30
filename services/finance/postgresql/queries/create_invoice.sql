INSERT INTO invoices AS invoice (
    organization_id,
    id,
    direction,
    number,
    invoice_date,
    due_date,
    own_company_id,
    own_company_name,
    counterparty_id,
    counterparty_name,
    own_bank_account_id,
    counterparty_bank_account_id,
    contract_id,
    contract_number,
    currency,
    note,
    archived,
    revision,
    net_minor,
    vat_minor,
    total_minor
)
SELECT $1::UUID, value.*
FROM (SELECT ($2::public.invoice_v1).*) AS value
ON CONFLICT (organization_id, id) DO NOTHING
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
