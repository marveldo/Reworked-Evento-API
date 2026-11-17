CREATE TABLE event_users (
    event_id      UUID        NOT NULL REFERENCES events(id) ON DELETE CASCADE,
    user_id       UUID        NOT NULL REFERENCES users(id)  ON DELETE CASCADE,
    registered_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (event_id, user_id)
);

CREATE INDEX event_users_user_id_idx ON event_users (user_id);
