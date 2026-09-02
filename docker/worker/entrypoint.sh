#!/bin/sh
set -e

CRON_FILE=/etc/cron.d/worker-cron

{
  echo "DB_PATH=${DB_PATH}"
  echo "SOURCES_JSON_PATH=${SOURCES_JSON_PATH}"
  echo "REQUEST_DATA_PATH=${REQUEST_DATA_PATH}"
  echo "USER_AGENT=${USER_AGENT}"
  echo "MAX_PAGES_TO_FETCH=${MAX_PAGES_TO_FETCH}"
  echo ""
  cat /app/cron/crontab.template
} > "$CRON_FILE"

chmod 0644 "$CRON_FILE"
crontab "$CRON_FILE"

echo "[INFO] Crontab zainstalowany z aktualnym środowiskiem, startuję cron..."
exec cron -f
