CREATE EXTENSION pgcrypto; -- Not needed if we are in version 13 or above

CREATE TABLE game (
  id SERIAL PRIMARY KEY,
  name VARCHAR(200) NOT NULL UNIQUE,
  creation_date TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE registration (
  id SERIAL PRIMARY KEY,
  game_id INTEGER REFERENCES game(id),
  email VARCHAR(254) NOT NULL,
  unregister_token UUID DEFAULT gen_random_uuid() UNIQUE,
  UNIQUE (email, game_id)
);
