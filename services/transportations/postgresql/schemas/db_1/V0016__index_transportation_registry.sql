-- Ключ сортировки совпадает с запросом: регистр не влияет на поиск номера.
-- UUID задаёт однозначный порядок для одинаковых и отсутствующих номеров.
CREATE INDEX transportations_registry_number_idx ON transportations
    (organization_id, (lower(COALESCE(container_number, '')) COLLATE "C"), id);
