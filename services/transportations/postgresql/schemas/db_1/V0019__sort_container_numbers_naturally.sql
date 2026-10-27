-- Поддерживает естественный порядок и для нестандартных номеров: ABC2 перед ABC10.
CREATE COLLATION public.registry_number_order (
    provider = icu,
    locale = 'und-u-kn-true',
    deterministic = true
);

DROP INDEX transportations_registry_number_idx;
CREATE INDEX transportations_registry_number_idx ON transportations (
    organization_id,
    (lower(COALESCE(container_number, '')) COLLATE public.registry_number_order),
    id
);
