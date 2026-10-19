-- Migration 0002: Seed Data (DOWN)
DELETE FROM users WHERE username IN ('john_doe', 'jane_smith', 'alex_turner', 'dev_lead', 'benchmark_bot');
