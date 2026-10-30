SELECT ROW(
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
)::public.invoice_v1,
    {sort_key} AS text_key,
    {date_key} AS date_key
FROM invoices AS invoice
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
