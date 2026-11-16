CREATE TABLE user_profiles (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    display_name TEXT NOT NULL,
    last_name TEXT,
    first_name TEXT,
    middle_name TEXT
);

CREATE TABLE user_identities (
    issuer TEXT NOT NULL,
    subject TEXT NOT NULL,
    profile_id UUID NOT NULL REFERENCES user_profiles(id),
    PRIMARY KEY (issuer, subject)
);

-- Предпочитаем запись со структурированным ФИО, затем с большей редакцией.
-- Идентификатор организации разрешает ничьи независимо от порядка строк.
WITH backfill AS MATERIALIZED (
    SELECT gen_random_uuid() AS profile_id, chosen.*
    FROM (
        SELECT DISTINCT ON (issuer, subject)
            issuer, subject, display_name, last_name, first_name, middle_name
        FROM memberships
        ORDER BY issuer, subject, (last_name IS NOT NULL AND first_name IS NOT NULL) DESC,
            revision DESC, organization_id
    ) AS chosen
), inserted AS (
    INSERT INTO user_profiles (id, display_name, last_name, first_name, middle_name)
    SELECT profile_id, display_name, last_name, first_name, middle_name FROM backfill
    RETURNING id
)
INSERT INTO user_identities (issuer, subject, profile_id)
SELECT issuer, subject, profile_id FROM backfill JOIN inserted ON inserted.id = backfill.profile_id;

ALTER TABLE memberships
    ADD CONSTRAINT memberships_user_identity_fk FOREIGN KEY (issuer, subject)
        REFERENCES user_identities (issuer, subject),
    DROP COLUMN display_name,
    DROP COLUMN last_name,
    DROP COLUMN first_name,
    DROP COLUMN middle_name;

CREATE INDEX memberships_user_identity_idx ON memberships (issuer, subject);
CREATE INDEX user_identities_profile_idx ON user_identities (profile_id);
CREATE INDEX user_profiles_registry_name_idx ON user_profiles (lower(display_name) COLLATE "C");
