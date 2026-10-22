-- Профиль общий: обновляем его копии во всех организациях данного пользователя.
UPDATE memberships
SET display_name = $3, last_name = $4, first_name = $5, middle_name = $6,
    revision = revision + 1
WHERE issuer = $1 AND subject = $2;
