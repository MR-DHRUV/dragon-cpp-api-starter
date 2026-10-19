-- Migration 0003: Add bio column to users (DOWN)
ALTER TABLE users DROP COLUMN IF EXISTS bio;
