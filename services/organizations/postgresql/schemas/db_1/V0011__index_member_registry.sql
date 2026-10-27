CREATE INDEX memberships_registry_name_idx
    ON memberships (organization_id, (lower(display_name) COLLATE "C"), subject);

CREATE INDEX memberships_registry_search_idx
    ON memberships USING GIN (
        (lower(display_name || ' ' || username || ' ' || email) COLLATE "C") public.gin_trgm_ops
    );
