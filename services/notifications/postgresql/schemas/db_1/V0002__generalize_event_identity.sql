-- Старые события прототипа не переносим. Личные уведомления сохраняются отдельно.
DROP TABLE notification_events;

CREATE TABLE notification_events (
    producer TEXT NOT NULL,
    organization_id UUID NOT NULL,
    event_id TEXT NOT NULL,
    event_type TEXT NOT NULL,
    event_version INTEGER NOT NULL CHECK (event_version > 0),
    payload JSONB NOT NULL,
    received_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (producer, organization_id, event_id)
);
