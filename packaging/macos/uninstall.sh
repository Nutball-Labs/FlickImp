#!/usr/bin/env bash
# uninstall.sh — Remove FlickImp for the current macOS user
#
# Usage:
#   ./uninstall.sh           — remove program + login item; KEEP config and database
#   ./uninstall.sh --purge   — also delete config, database and log

set -euo pipefail

LABEL="com.nutball-labs.flickimp"
DATA="$HOME/Library/Application Support/flickimp"
APP="$DATA/app"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
LOG="$HOME/Library/Logs/flickimp.log"
DOMAIN="gui/$(id -u)"

PURGE=false
[[ "${1:-}" == "--purge" ]] && PURGE=true

echo "--- Stopping FlickImp ---"
launchctl bootout "$DOMAIN/$LABEL" 2>/dev/null || true
rm -f "$PLIST"

echo "--- Removing $APP ---"
rm -rf "$APP"

if $PURGE; then
    read -rp "Delete your FlickImp database and config in $DATA? This cannot be undone. [y/N] " ans
    if [[ "$ans" =~ ^[Yy] ]]; then
        rm -rf "$DATA" "$LOG"
        echo "Config, database and log deleted."
    else
        echo "Kept $DATA"
    fi
else
    echo "Kept config and database in: $DATA"
    echo "(run with --purge to delete them)"
fi

echo "FlickImp uninstalled."

# SN: 00005
