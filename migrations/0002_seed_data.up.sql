-- Migration 0002: Seed Data (UP)
INSERT INTO users (username, email, full_name, is_active) VALUES
    ('john_doe', 'john.doe@example.com', 'John Doe', true),
    ('jane_smith', 'jane.smith@example.com', 'Jane Smith', true),
    ('alex_turner', 'alex.turner@example.com', 'Alex Turner', true),
    ('dev_lead', 'devlead@example.com', 'Development Lead', true),
    ('benchmark_bot', 'benchmark@example.com', 'Benchmark Test Bot', true)
ON CONFLICT (username) DO NOTHING;
