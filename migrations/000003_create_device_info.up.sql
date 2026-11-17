CREATE TABLE device_info (
    id            UUID          PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id       UUID          NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    ip_address    VARCHAR(120),
    device_name   VARCHAR(120),
    device_os     VARCHAR(600),
    device_type   VARCHAR(600),
    access_token  VARCHAR(1500),
    refresh_token VARCHAR(1500),
    created_at    TIMESTAMPTZ   NOT NULL DEFAULT NOW()
);

CREATE INDEX device_info_user_id_idx ON device_info (user_id);
