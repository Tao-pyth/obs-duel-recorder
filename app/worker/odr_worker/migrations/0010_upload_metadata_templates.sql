-- v1.1.3 migration 0010_upload_metadata_templates
-- Purpose: store editable upload description and tag templates with match metadata.

ALTER TABLE matches ADD COLUMN description_template TEXT NOT NULL DEFAULT '';
ALTER TABLE matches ADD COLUMN tags_template TEXT NOT NULL DEFAULT '';
