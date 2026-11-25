CREATE TYPE sales_planned_container_v1 AS (
    container_type_code TEXT,
    container_ownership TEXT,
    planned_quantity INTEGER
);

ALTER TABLE sales_inquiries ADD COLUMN planned_containers sales_planned_container_v1[];
