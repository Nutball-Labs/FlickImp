#!/usr/bin/env bash
# RPM %preun scriptlet — runs before package is removed (not on upgrade)
if [ "$1" -eq 0 ]; then
    systemctl stop    flickimp 2>/dev/null || true
    systemctl disable flickimp 2>/dev/null || true
fi
