CREATE TABLE events (
    id          UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
    event_name  VARCHAR(250),
    date        DATE,
    description TEXT,
    start_date  DATE,
    start_time  TIME,
    "time"      TIME,
    price       INTEGER      DEFAULT 0,
    location    VARCHAR(400),
    event_image VARCHAR(500),
    capacity    INTEGER      DEFAULT 0,
    created_by  UUID         REFERENCES users(id) ON DELETE SET NULL,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    updated_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

CREATE INDEX events_created_at_idx ON events (created_at DESC);
CREATE INDEX events_created_by_idx ON events (created_by);

CREATE TRIGGER events_set_updated_at
    BEFORE UPDATE ON events
    FOR EACH ROW EXECUTE FUNCTION set_updated_at();
