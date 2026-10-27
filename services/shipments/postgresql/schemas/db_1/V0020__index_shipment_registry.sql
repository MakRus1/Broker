-- Основные порядки реестра и переход к следующей странице без OFFSET.
CREATE INDEX shipments_registry_date_idx ON shipments (organization_id, shipment_date, id);
CREATE INDEX shipments_registry_number_idx ON shipments
    (organization_id, (lower(number) COLLATE "C"), id);
