#!/bin/sh
set -e

DB_FILE="${DB_PATH:-/var/www/html/database/cybersecurityfeed.sqlite}"
mkdir -p "$(dirname "$DB_FILE")"

echo "[INIT] Inicjalizacja bazy: $DB_FILE"
sqlite3 "$DB_FILE" < /var/www/html/schema.sql

exec apache2-foreground
