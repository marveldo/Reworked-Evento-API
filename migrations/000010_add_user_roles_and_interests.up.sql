ALTER TABLE users ADD COLUMN role VARCHAR(20) NOT NULL DEFAULT 'organizer';
ALTER TABLE users ALTER COLUMN role SET DEFAULT 'attendee';
ALTER TABLE users ADD CONSTRAINT users_role_check CHECK (role IN ('attendee', 'organizer'));

ALTER TABLE users ADD COLUMN onboarded BOOLEAN NOT NULL DEFAULT TRUE;
ALTER TABLE users ALTER COLUMN onboarded SET DEFAULT FALSE;

CREATE TABLE user_interests (
    user_id      UUID        NOT NULL REFERENCES users(id)      ON DELETE CASCADE,
    event_tag_id UUID        NOT NULL REFERENCES event_tags(id) ON DELETE CASCADE,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (user_id, event_tag_id)
);

CREATE INDEX user_interests_event_tag_id_idx ON user_interests (event_tag_id);
