CREATE INDEX counterparties_registry_name_idx ON counterparties
    (organization_id, (lower(name) COLLATE "C"), id);
