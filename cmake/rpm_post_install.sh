#!/usr/bin/env bash
# RPM %post scriptlet — runs after package files are installed
systemctl daemon-reload 2>/dev/null || true
echo ""
echo "FlickImp installed.  To start the service:"
echo "  systemctl enable --now flickimp"
echo "  flickimp-config"
echo ""
