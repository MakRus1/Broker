INSERT INTO hello_schema.users(name, count)
VALUES ($1, 1)
ON CONFLICT (name) DO UPDATE SET count = users.count + 1
RETURNING users.count;
