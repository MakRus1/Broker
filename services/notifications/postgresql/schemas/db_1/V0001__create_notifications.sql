-- Сохранённые события не удаляются после обработки: это журнал дедупликации.
CREATE TABLE notification_events (
    producer TEXT NOT NULL,
    organization_id UUID NOT NULL,
    shipment_id UUID NOT NULL,
    revision BIGINT NOT NULL CHECK (revision > 0),
    payload JSONB NOT NULL,
    received_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (producer, organization_id, shipment_id, revision)
);

CREATE TABLE notifications (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    organization_id UUID NOT NULL,
    recipient_issuer TEXT NOT NULL,
    recipient_subject TEXT NOT NULL,
    shipment_id UUID NOT NULL,
    shipment_number TEXT NOT NULL,
    shipment_revision BIGINT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    read_at TIMESTAMPTZ,
    dismissed_at TIMESTAMPTZ,
    UNIQUE (organization_id, recipient_issuer, recipient_subject, shipment_id, shipment_revision)
);

CREATE INDEX notifications_recipient_history
    ON notifications (organization_id, recipient_issuer, recipient_subject, created_at DESC, id DESC);
