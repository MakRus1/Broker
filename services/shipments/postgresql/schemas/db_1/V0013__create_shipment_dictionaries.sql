CREATE TABLE shipment_dictionary_values (
    dictionary TEXT NOT NULL,
    code TEXT NOT NULL,
    name TEXT NOT NULL,
    position INTEGER NOT NULL,
    active BOOLEAN NOT NULL DEFAULT TRUE,
    PRIMARY KEY (dictionary, code)
);

CREATE TYPE public.shipment_dictionary_value_v1 AS (
    dictionary TEXT,
    code TEXT,
    name TEXT
);

INSERT INTO shipment_dictionary_values (dictionary, code, name, position)
VALUES
    ('incoterms', 'EXW', 'EXW', 0),
    ('incoterms', 'FCA', 'FCA', 1),
    ('incoterms', 'CPT', 'CPT', 2),
    ('incoterms', 'CIP', 'CIP', 3),
    ('incoterms', 'DAP', 'DAP', 4),
    ('incoterms', 'DPU', 'DPU', 5),
    ('incoterms', 'DDP', 'DDP', 6),
    ('incoterms', 'FAS', 'FAS', 7),
    ('incoterms', 'FOB', 'FOB', 8),
    ('incoterms', 'CFR', 'CFR', 9),
    ('incoterms', 'CIF', 'CIF', 10),
    ('carriage_terms', 'FIFO', 'FIFO', 0),
    ('carriage_terms', 'FILO', 'FILO', 1),
    ('carriage_terms', 'LIFO', 'LIFO', 2),
    ('carriage_terms', 'LILO', 'LILO', 3),
    ('loading', 'FTL', 'FTL', 0),
    ('loading', 'LTL', 'LTL', 1),
    ('loading', 'FCL', 'FCL', 2),
    ('loading', 'LCL', 'LCL', 3),
    ('route_type', 'sea', 'Море', 0),
    ('route_type', 'international_rail', 'Международное ЖД', 1),
    ('route_type', 'international_road', 'Международное авто', 2),
    ('route_type', 'rail', 'ЖД РФ', 3),
    ('route_type', 'road', 'Авто', 4),
    ('route_type', 'pickup', 'Вывоз', 5),
    ('route_type', 'air', 'Авиа', 6);
