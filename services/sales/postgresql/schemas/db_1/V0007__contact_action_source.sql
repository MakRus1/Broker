UPDATE sales_history AS history
SET created_at = contact.created_at
FROM sales_contacts AS contact
WHERE history.organization_id = contact.organization_id AND history.id = contact.id
    AND history.entity = 'contacts' AND history.revision = 1;

CREATE TABLE sales_card_action_sources (
    organization_id UUID NOT NULL,
    card_id UUID NOT NULL,
    contact_id UUID NOT NULL,
    PRIMARY KEY (organization_id, card_id),
    FOREIGN KEY (organization_id, card_id) REFERENCES sales_cards (organization_id, id),
    FOREIGN KEY (organization_id, contact_id) REFERENCES sales_contacts (organization_id, id)
);
-- Восстанавливаем связь существующих действий с исходной версией контакта.
INSERT INTO sales_card_action_sources (organization_id, card_id, contact_id)
SELECT DISTINCT ON (card.organization_id, card.id)
    card.organization_id, card.id, contact.id
FROM sales_cards AS card
JOIN sales_contacts AS contact
    ON contact.organization_id = card.organization_id AND contact.card_id = card.id
JOIN sales_history AS history
    ON history.organization_id = contact.organization_id AND history.entity = 'contacts'
    AND history.id = contact.id
WHERE card.next_action IS NOT NULL
    AND card.next_action = history.snapshot->>'next_action'
    AND card.next_action_date IS NOT DISTINCT FROM (history.snapshot->>'next_action_date')::DATE
ORDER BY card.organization_id, card.id, contact.created_at DESC, contact.id DESC;
