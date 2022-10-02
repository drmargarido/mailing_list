ALTER TABLE registration
  ADD COLUMN email_confirmed BOOLEAN NOT NULL DEFAULT false,
  ADD COLUMN creation_date TIMESTAMPTZ NOT NULL DEFAULT NOW();

UPDATE registration SET email_confirmed = true;
