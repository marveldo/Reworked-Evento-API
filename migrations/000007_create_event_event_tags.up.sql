CREATE TABLE event_event_tags (
    event_id     UUID NOT NULL REFERENCES events(id)     ON DELETE CASCADE,
    event_tag_id UUID NOT NULL REFERENCES event_tags(id) ON DELETE CASCADE,
    PRIMARY KEY (event_id, event_tag_id)
);

CREATE INDEX event_event_tags_event_tag_id_idx ON event_event_tags (event_tag_id);
