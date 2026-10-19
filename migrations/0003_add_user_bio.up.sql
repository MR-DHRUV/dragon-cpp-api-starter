-- Migration 0003: Add bio column to users (UP)
ALTER TABLE users ADD COLUMN IF NOT EXISTS bio TEXT DEFAULT '';
