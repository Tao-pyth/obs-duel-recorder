-- v1.1.10 migration 0011_deck_sequence_numbers
-- Purpose: persist a per-own-deck usage sequence separately from match and queue ids.

ALTER TABLE matches ADD COLUMN deck_sequence_number INTEGER NOT NULL DEFAULT 0;

UPDATE matches
SET deck_sequence_number = (
  SELECT COUNT(*)
  FROM matches AS previous
  WHERE LOWER(TRIM(previous.deck_name)) = LOWER(TRIM(matches.deck_name))
    AND TRIM(previous.deck_name) != ''
    AND previous.id <= matches.id
)
WHERE TRIM(deck_name) != '';

CREATE INDEX IF NOT EXISTS idx_matches_deck_sequence
ON matches(deck_name, deck_sequence_number);
