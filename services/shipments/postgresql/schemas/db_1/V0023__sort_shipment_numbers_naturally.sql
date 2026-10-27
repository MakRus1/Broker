-- Числовые части номера сравниваются как числа: S-2 раньше S-10.
-- Регистр по-прежнему нормализуется в запросе; одинаковые ключи разделяет id.
CREATE COLLATION public.registry_number_order (
    provider = icu,
    locale = 'und-u-kn-true',
    deterministic = true
);

DROP INDEX shipments_registry_number_idx;
CREATE INDEX shipments_registry_number_idx ON shipments (
    organization_id,
    (lower(number) COLLATE public.registry_number_order),
    id
);
