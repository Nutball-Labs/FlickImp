#!/usr/bin/env bash
# RPM %pre scriptlet — runs before package files are installed
getent group  flickimp >/dev/null || groupadd -r flickimp
getent passwd flickimp >/dev/null || useradd  -r -s /sbin/nologin \
    -d /var/lib/flickimp -g flickimp \
    -c "FlickImp service account" flickimp

install -d -m 755 -o root     -g root     /etc/flickimp
install -d -m 750 -o flickimp -g flickimp /var/lib/flickimp
install -d -m 750 -o flickimp -g flickimp /var/lib/flickimp/db
install -d -m 755 -o root     -g root     /var/lib/flickimp/web

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
else
    cp "$CONFIG" "${CONFIG}.bak"
    chmod 644 "${CONFIG}.bak"
    chown root:flickimp "${CONFIG}.bak"
fi

# SN: 00003
