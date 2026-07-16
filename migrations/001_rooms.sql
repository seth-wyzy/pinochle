CREATE TABLE IF NOT EXISTS rooms (
    code CHAR(8) PRIMARY KEY,
    snapshot JSONB NOT NULL,
    version INTEGER NOT NULL DEFAULT 1,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    expires_at TIMESTAMPTZ NOT NULL
);

CREATE INDEX IF NOT EXISTS rooms_expires_at_idx ON rooms (expires_at);

CREATE TABLE IF NOT EXISTS room_seats (
    room_code CHAR(8) NOT NULL REFERENCES rooms(code) ON DELETE CASCADE,
    seat SMALLINT NOT NULL CHECK (seat BETWEEN 0 AND 3),
    name VARCHAR(24) NOT NULL,
    ai BOOLEAN NOT NULL,
    token_hash CHAR(64),
    PRIMARY KEY (room_code, seat),
    UNIQUE (room_code, token_hash)
);

CREATE TABLE IF NOT EXISTS room_events (
    id BIGSERIAL PRIMARY KEY,
    room_code CHAR(8) NOT NULL REFERENCES rooms(code) ON DELETE CASCADE,
    text TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS room_events_room_created_idx ON room_events (room_code, created_at);
