CREATE TABLE event_tags (
    id       UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
    tag_name VARCHAR(550),
    CONSTRAINT event_tags_tag_name_key UNIQUE (tag_name)
);
