#!/usr/bin/env bash
# install-service.sh — Set up the flickimp system user and install the service
# Use this when installing from the source tree (not from an RPM/DEB).
# The RPM/DEB packages run this setup automatically via post-install scriptlets.
#
# Must be run as root: sudo ./scripts/install-service.sh

set -euo pipefail

if [[ $EUID -ne 0 ]]; then
    echo "ERROR: Run this script as root: sudo ./scripts/install-service.sh"
    exit 1
fi

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ="$(cd "$SELF_DIR/.." && pwd)"

echo "--- Creating flickimp system user and group ---"
getent group  flickimp >/dev/null || groupadd -r flickimp
getent passwd flickimp >/dev/null || useradd  -r -s /sbin/nologin \
    -d /var/lib/flickimp -g flickimp \
    -c "FlickImp service account" flickimp

echo "--- Setting up directories ---"
install -d -m 755 -o root     -g root     /etc/flickimp
install -d -m 750 -o flickimp -g flickimp /var/lib/flickimp
install -d -m 750 -o flickimp -g flickimp /var/lib/flickimp/db
install -d -m 755 -o root     -g root     /var/lib/flickimp/web

echo "--- Seeding config ---"
CONFIG=/etc/flickimp/fi_config.json
if [[ ! -f "$CONFIG" ]]; then
    cat > "$CONFIG" << 'EOF'
{
  "port": 8647,
  "fi_db_path": "/var/lib/flickimp/db",
  "fi_web_root": "",
  "tmdb_api_key": "",
  "tmdb_bearer_token": ""
}
EOF
    chmod 644 "$CONFIG"
    chown root:flickimp "$CONFIG"
    echo "  Created $CONFIG with defaults"
else
    cp "$CONFIG" "${CONFIG}.bak"
    chmod 644 "${CONFIG}.bak"
    chown root:flickimp "${CONFIG}.bak"
    echo "  Existing config preserved (backup → ${CONFIG}.bak)"
fi

echo "--- Installing service unit ---"
install -m 644 "$PROJ/service/flickimp.service" \
    /etc/systemd/system/flickimp.service
systemctl daemon-reload

echo ""
echo "Done. Next steps:"
echo "  systemctl enable --now flickimp   # start now and at every boot"
echo "  systemctl status flickimp         # verify it's running"
echo "  flickimp-config                   # open the GUI configurator"

# SN: 00003
