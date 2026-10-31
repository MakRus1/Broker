CREATE TABLE finance_settings (
    organization_id UUID PRIMARY KEY,
    reporting_currency TEXT NOT NULL,
    revision BIGINT NOT NULL
);
CREATE TABLE exchange_rate_days (
    requested_date DATE PRIMARY KEY,
    rate_date DATE NOT NULL
);
CREATE TABLE exchange_rates (
    requested_date DATE NOT NULL REFERENCES exchange_rate_days(requested_date),
    currency TEXT NOT NULL,
    rub_millionths BIGINT NOT NULL,
    nominal BIGINT NOT NULL,
    PRIMARY KEY (requested_date, currency)
);
ALTER TABLE service_charges ADD COLUMN service_date DATE;
ALTER TYPE public.service_charge_v1 ADD ATTRIBUTE service_date DATE;
ALTER TABLE service_charges ADD COLUMN invoice_currency TEXT;
ALTER TYPE public.service_charge_v1 ADD ATTRIBUTE invoice_currency TEXT;
ALTER TABLE service_charges ADD COLUMN invoice_rate_millionths BIGINT;
ALTER TYPE public.service_charge_v1 ADD ATTRIBUTE invoice_rate_millionths BIGINT;
ALTER TABLE service_charges ADD COLUMN invoice_rate_date DATE;
ALTER TYPE public.service_charge_v1 ADD ATTRIBUTE invoice_rate_date DATE;
ALTER TABLE service_charges ADD COLUMN invoice_rate_source TEXT;
ALTER TYPE public.service_charge_v1 ADD ATTRIBUTE invoice_rate_source TEXT;
ALTER TABLE service_charges ADD COLUMN invoice_net_minor BIGINT;
ALTER TYPE public.service_charge_v1 ADD ATTRIBUTE invoice_net_minor BIGINT;
ALTER TABLE service_charges ADD COLUMN invoice_vat_minor BIGINT;
ALTER TYPE public.service_charge_v1 ADD ATTRIBUTE invoice_vat_minor BIGINT;
ALTER TABLE service_charges ADD COLUMN invoice_total_minor BIGINT;
ALTER TYPE public.service_charge_v1 ADD ATTRIBUTE invoice_total_minor BIGINT;

-- Для существующих услуг дата берётся из первой записи истории.
UPDATE service_charges AS charge
SET service_date = COALESCE((SELECT MIN(occurred_at)::DATE FROM service_charge_history AS history
    WHERE history.organization_id = charge.organization_id AND history.service_charge_id = charge.id), CURRENT_DATE),
    invoice_currency = currency, invoice_rate_millionths = 1000000,
    invoice_rate_source = 'identity', invoice_net_minor = net_minor,
    invoice_vat_minor = vat_minor, invoice_total_minor = total_minor;
UPDATE service_charges SET invoice_rate_date = service_date;
UPDATE invoice_service_charges AS line
SET snapshot = ROW(
    (line.snapshot).id,
    (line.snapshot).shipment_id,
    (line.snapshot).transportation_id,
    (line.snapshot).label_id,
    (line.snapshot).label_name,
    (line.snapshot).direction,
    (line.snapshot).unit,
    (line.snapshot).counterparty_id,
    (line.snapshot).counterparty_name,
    (line.snapshot).contract_id,
    (line.snapshot).contract_number,
    (line.snapshot).quantity_milli,
    (line.snapshot).price_minor,
    (line.snapshot).currency,
    (line.snapshot).vat_mode,
    (line.snapshot).vat_rate_basis_points,
    (line.snapshot).net_minor,
    (line.snapshot).vat_minor,
    (line.snapshot).total_minor,
    (line.snapshot).note,
    (line.snapshot).archived,
    (line.snapshot).revision,
    (line.snapshot).price_mode,
    charge.service_date,
    (line.snapshot).currency,
    1000000,
    charge.service_date,
    'identity',
    (line.snapshot).net_minor,
    (line.snapshot).vat_minor,
    (line.snapshot).total_minor
)::public.service_charge_v1
FROM invoices AS invoice
JOIN service_charges AS charge ON charge.organization_id = invoice.organization_id
WHERE invoice.organization_id = line.organization_id
    AND invoice.id = line.invoice_id AND charge.id = line.service_charge_id;
ALTER TABLE service_charges ALTER COLUMN service_date SET NOT NULL;
ALTER TABLE service_charges ALTER COLUMN invoice_currency SET NOT NULL;
ALTER TABLE service_charges ALTER COLUMN invoice_rate_millionths SET NOT NULL;
ALTER TABLE service_charges ALTER COLUMN invoice_rate_date SET NOT NULL;
ALTER TABLE service_charges ALTER COLUMN invoice_rate_source SET NOT NULL;
ALTER TABLE service_charges ALTER COLUMN invoice_net_minor SET NOT NULL;
ALTER TABLE service_charges ALTER COLUMN invoice_vat_minor SET NOT NULL;
ALTER TABLE service_charges ALTER COLUMN invoice_total_minor SET NOT NULL;
