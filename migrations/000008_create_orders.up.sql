CREATE TABLE orders (
    id         UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id    UUID         REFERENCES users(id)  ON DELETE SET NULL,
    event_id   UUID         REFERENCES events(id) ON DELETE SET NULL,
    email      VARCHAR(254) NOT NULL,
    amount     BIGINT       NOT NULL,
    currency   VARCHAR(3)   NOT NULL DEFAULT 'NGN',
    reference  VARCHAR(100) NOT NULL,
    status     VARCHAR(20)  NOT NULL DEFAULT 'pending',
    paid_at    TIMESTAMPTZ,
    created_at TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    CONSTRAINT orders_reference_key UNIQUE (reference),
    CONSTRAINT orders_amount_positive CHECK (amount > 0),
    CONSTRAINT orders_status_check CHECK (status IN ('pending', 'success', 'failed', 'abandoned', 'refunded'))
);

CREATE INDEX orders_user_id_idx  ON orders (user_id);
CREATE INDEX orders_event_id_idx ON orders (event_id);
CREATE UNIQUE INDEX orders_one_success_per_attendee ON orders (event_id, user_id) WHERE status = 'success';

CREATE TRIGGER orders_set_updated_at
    BEFORE UPDATE ON orders
    FOR EACH ROW EXECUTE FUNCTION set_updated_at();
