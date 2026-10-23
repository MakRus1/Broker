ALTER TABLE transportations ADD COLUMN container_number TEXT;

CREATE TYPE public.transportation_v4 AS (
    id UUID,
    revision BIGINT,
    shipment_id UUID,
    item_id UUID,
    operation_id UUID,
    container_type TEXT,
    ownership TEXT,
    container_number TEXT,
    type TEXT,
    hazardous BOOLEAN
);

-- Старый числовой number оставлен на переходный период для старых реплик.
-- Новый код не использует его для отображения или идентификации перевозок.
