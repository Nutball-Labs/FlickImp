#!/usr/bin/env bash
# install.sh — Install FlickImp for the current macOS user
#
# Usage (from the unpacked flickimp-X.Y.Z-macOS folder):
#   ./install.sh
#
# Installs (no sudo needed):
#   ~/Library/Application Support/flickimp/app/     flickimp binary + web/
#   ~/Library/Application Support/flickimp/         fi_config.json + flickimp.db (kept on upgrade)
#   ~/Library/LaunchAgents/com.nutball-labs.flickimp.plist   starts FlickImp at login
#   ~/Library/Logs/flickimp.log                     daemon output
#
# Re-running install.sh upgrades in place; your config and database are kept.

set -euo pipefail

SRC="$(cd "$(dirname "$0")" && pwd)"
LABEL="com.nutball-labs.flickimp"
DATA="$HOME/Library/Application Support/flickimp"
APP="$DATA/app"
CONFIG="$DATA/fi_config.json"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
LOG="$HOME/Library/Logs/flickimp.log"
DOMAIN="gui/$(id -u)"

if [[ ! -x "$SRC/flickimp" || ! -d "$SRC/web" ]]; then
    echo "ERROR: run install.sh from the unpacked FlickImp folder (flickimp + web/ not found)." >&2
    exit 1
fi

# Clear Gatekeeper quarantine on the unpacked folder so the unsigned binary can run
xattr -dr com.apple.quarantine "$SRC" 2>/dev/null || true

echo "Installing FlickImp $("$SRC/flickimp" --version 2>/dev/null | awk '{print $2}') for $USER"
echo ""

# ── Stop a running instance ───────────────────────────────────────────────────
if launchctl print "$DOMAIN/$LABEL" &>/dev/null; then
    echo "--- Stopping running FlickImp ---"
    launchctl bootout "$DOMAIN/$LABEL" 2>/dev/null || true
    sleep 1
fi

# ── Copy program files ────────────────────────────────────────────────────────
echo "--- Copying files to $APP ---"
mkdir -p "$APP" "$HOME/Library/LaunchAgents" "$HOME/Library/Logs"
rm -rf "$APP/web"
cp -f "$SRC/flickimp" "$APP/flickimp"
cp -R "$SRC/web" "$APP/web"
cp -f "$SRC/uninstall.sh" "$APP/uninstall.sh"
chmod 755 "$APP/flickimp" "$APP/uninstall.sh"

# Downloaded archives are quarantined by Gatekeeper; the binary is unsigned
# (ad-hoc), so clear the flag or launchd will refuse to start it.
xattr -dr com.apple.quarantine "$APP" 2>/dev/null || true

# ── Config (first install only) ───────────────────────────────────────────────
if [[ ! -f "$CONFIG" ]]; then
    echo ""
    echo "--- First-time setup ---"
    echo "FlickImp looks up shows and movies via TMDB. Get a free API Read Access"
    echo "Token at https://www.themoviedb.org/settings/api (the long 'eyJ...' one)."
    echo ""
    read -rp "TMDB API Read Access Token (Enter to skip, edit later): " TOKEN
    read -rp "Web UI port [8647]: " PORT
    PORT="${PORT:-8647}"
    if ! [[ "$PORT" =~ ^[0-9]+$ ]]; then
        echo "Port must be a number — using 8647."
        PORT=8647
    fi
    cat > "$CONFIG" <<EOF
{
  "port": $PORT,
  "fi_web_root": "",
  "fi_db_path": "",
  "tmdb_api_key": "",
  "tmdb_bearer_token": "$TOKEN"
}
EOF
    chmod 600 "$CONFIG"
    echo "Wrote $CONFIG"
else
    echo "--- Keeping existing config: $CONFIG ---"
fi

PORT=$(sed -nE 's/.*"port"[[:space:]]*:[[:space:]]*([0-9]+).*/\1/p' "$CONFIG" | head -1)
PORT="${PORT:-8647}"

# ── launchd LaunchAgent ───────────────────────────────────────────────────────
echo "--- Registering login item ($PLIST) ---"
cat > "$PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>$LABEL</string>
    <key>ProgramArguments</key>
    <array>
        <string>$APP/flickimp</string>
        <string>--log</string>
        <string>$LOG</string>
    </array>
    <key>WorkingDirectory</key>
    <string>$APP</string>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <dict>
        <key>SuccessfulExit</key>
        <false/>
    </dict>
    <key>ThrottleInterval</key>
    <integer>30</integer>
    <key>ProcessType</key>
    <string>Background</string>
</dict>
</plist>
EOF

launchctl bootstrap "$DOMAIN" "$PLIST"

# ── Verify ────────────────────────────────────────────────────────────────────
URL="http://localhost:$PORT"
echo ""
printf "Waiting for FlickImp to answer on %s " "$URL"
for _ in 1 2 3 4 5 6 7 8 9 10; do
    if curl -fs -o /dev/null "$URL/api/about"; then
        echo "— OK"
        echo ""
        echo "FlickImp is running and will start automatically at login."
        echo "  Web UI:    $URL"
        echo "  Config:    $CONFIG"
        echo "  Log:       $LOG"
        echo "  CLI:       \"$APP/flickimp\" --check"
        echo "  Uninstall: \"$APP/uninstall.sh\""
        read -rp "Open the web UI now? [Y/n] " ans
        [[ "${ans:-y}" =~ ^[Nn] ]] || open "$URL"
        exit 0
    fi
    printf "."
    sleep 1
done
echo ""
echo "WARNING: FlickImp did not respond within 10 seconds. Last log lines:" >&2
tail -n 20 "$LOG" 2>/dev/null >&2 || true
exit 1

# SN: 00005
