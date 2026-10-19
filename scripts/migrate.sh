#!/usr/bin/env bash
set -e

DIRECTION="${1:-up}"
STEPS="${2:-1}"
CONTAINER_NAME="${CONTAINER_NAME:-postgres_db}"
DB_USER="${DB_USER:-api_user}"
DB_NAME="${DB_NAME:-api_db}"
MIGRATIONS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../migrations" && pwd)"

echo "=========================================="
echo "   Database Migration Runner (${DIRECTION^^})    "
echo "=========================================="

if ! docker inspect --format='{{json .State.Running}}' "$CONTAINER_NAME" 2>/dev/null | grep -q 'true'; then
    echo "[ERROR] PostgreSQL container '$CONTAINER_NAME' is not running!"
    echo "Please start it first using 'make db' or 'docker compose up -d'."
    exit 1
fi

docker exec -i "$CONTAINER_NAME" psql -U "$DB_USER" -d "$DB_NAME" -v ON_ERROR_STOP=1 -q <<EOF
CREATE TABLE IF NOT EXISTS schema_migrations (
    version VARCHAR(128) PRIMARY KEY,
    applied_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
EOF

if [ "$DIRECTION" = "up" ]; then
    APPLIED=$(docker exec -i "$CONTAINER_NAME" psql -U "$DB_USER" -d "$DB_NAME" -t -A -c "SELECT version FROM schema_migrations ORDER BY version ASC;" 2>/dev/null || true)
    APPLIED_COUNT=0
    SKIPPED_COUNT=0

    for file in $(ls "$MIGRATIONS_DIR"/*.up.sql 2>/dev/null | sort); do
        [ -f "$file" ] || continue
        filename=$(basename "$file")
        version="${filename%.up.sql}"

        if echo "$APPLIED" | grep -qx "$version"; then
            echo "  [SKIP]  $filename (already applied)"
            SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
        else
            printf "  [APPLY UP] %s..." "$filename"
            docker exec -i "$CONTAINER_NAME" psql -U "$DB_USER" -d "$DB_NAME" -v ON_ERROR_STOP=1 -q <<EOF
SET client_min_messages = WARNING;
BEGIN;
$(cat "$file")
INSERT INTO schema_migrations (version) VALUES ('$version') ON CONFLICT (version) DO NOTHING;
COMMIT;
EOF
            echo " [OK]"
            APPLIED_COUNT=$((APPLIED_COUNT + 1))
        fi
    done

    echo "=========================================="
    echo "Migrations UP summary: $APPLIED_COUNT applied, $SKIPPED_COUNT up to date."
    echo "=========================================="

elif [ "$DIRECTION" = "down" ]; then
    APPLIED=$(docker exec -i "$CONTAINER_NAME" psql -U "$DB_USER" -d "$DB_NAME" -t -A -c "SELECT version FROM schema_migrations ORDER BY version DESC;" 2>/dev/null || true)
    if [ -z "$APPLIED" ]; then
        echo "[INFO] No migrations to revert."
        exit 0
    fi

    COUNT=0
    for version in $APPLIED; do
        [ -n "$version" ] || continue
        if [ "$COUNT" -ge "$STEPS" ]; then
            break
        fi
        down_file="$MIGRATIONS_DIR/$version.down.sql"
        if [ ! -f "$down_file" ]; then
            echo "[ERROR] Down migration $down_file not found!"
            exit 1
        fi
        printf "  [REVERT DOWN] %s.down.sql..." "$version"
        docker exec -i "$CONTAINER_NAME" psql -U "$DB_USER" -d "$DB_NAME" -v ON_ERROR_STOP=1 -q <<EOF
SET client_min_messages = WARNING;
BEGIN;
$(cat "$down_file")
DELETE FROM schema_migrations WHERE version = '$version';
COMMIT;
EOF
        echo " [OK]"
        COUNT=$((COUNT + 1))
    done

    echo "=========================================="
    echo "Migrations DOWN summary: $COUNT migration(s) reverted."
    echo "=========================================="
fi
