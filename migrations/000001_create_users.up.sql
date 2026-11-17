CREATE OR REPLACE FUNCTION set_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TABLE users (
    id            UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
    full_name     VARCHAR(255),
    email         VARCHAR(254) NOT NULL,
    password      TEXT         NOT NULL,
    last_login    TIMESTAMPTZ,
    is_superuser  BOOLEAN      NOT NULL DEFAULT FALSE,
    is_verified   BOOLEAN      NOT NULL DEFAULT FALSE,
    is_staff      BOOLEAN      NOT NULL DEFAULT FALSE,
    is_admin      BOOLEAN      NOT NULL DEFAULT FALSE,
    profile_img   VARCHAR(500) NOT NULL DEFAULT 'https://hngtask2bucket.s3.amazonaws.com/screenshots/pngtree-beautiful-profile-line-vector-icon-png-image_2035279.jpg',
    auth_provider VARCHAR(50)  NOT NULL DEFAULT 'email',
    location      VARCHAR(500),
    bio           TEXT,
    website       VARCHAR(200),
    facebook      VARCHAR(200),
    twitter       VARCHAR(200),
    instagram     VARCHAR(200),
    created_at    TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    updated_at    TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    CONSTRAINT users_email_key UNIQUE (email)
);

CREATE TRIGGER users_set_updated_at
    BEFORE UPDATE ON users
    FOR EACH ROW EXECUTE FUNCTION set_updated_at();
