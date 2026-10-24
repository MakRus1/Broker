CREATE TABLE railway_stations (
    esr_code TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    country_code TEXT NOT NULL,
    archived BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TYPE public.railway_station_v1 AS (
    esr_code TEXT,
    name TEXT,
    country_code TEXT
);

-- ЖД-станции хранятся только в rail, свободные пункты больше не используются.
UPDATE route_segments
SET document = document - 'origin' - 'destination'
WHERE document ->> 'mode' IN ('rail', 'international_rail');
