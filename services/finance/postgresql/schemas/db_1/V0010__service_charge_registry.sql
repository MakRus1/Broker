-- Страница реестра по дате в пределах организации; id стабилизирует порядок равных дат.
CREATE INDEX service_charges_registry_date
    ON service_charges (organization_id, service_date, id);
