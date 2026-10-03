#!/usr/bin/env bash
# RPM %post scriptlet — runs after package files are installed
# $1 = number of package instances after this transaction: 1 = fresh install,
# 2+ = upgrade. (RPM runs scriptlets with /bin/sh, so keep this POSIX.)
systemctl daemon-reload 2>/dev/null || true

if [ "${1:-1}" -ge 2 ]; then
    # Upgrade: restart the service if it's running so the new binary and web
    # files take effect. try-restart leaves a stopped/disabled service alone.
    # Done here rather than in %postun because %postun comes from the *old*
    # package, so a fix there would only apply from the following upgrade.
    if systemctl try-restart flickimp 2>/dev/null; then
        if systemctl is-active --quiet flickimp 2>/dev/null; then
            echo "FlickImp upgraded; service restarted."
        else
            echo "FlickImp upgraded (service not running; start it with: systemctl start flickimp)."
        fi
    fi
else
    echo ""
    echo "FlickImp installed.  To start the service:"
    echo "  systemctl enable --now flickimp"
    echo "  flickimp-config"
    echo ""
fi

# SN: 00006
