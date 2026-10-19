-- Migration 0001: Initial Schema (DOWN)
DROP TABLE IF EXISTS users CASCADE;
DROP EXTENSION IF EXISTS "uuid-ossp";
