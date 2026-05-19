#!/usr/bin/env bash
# RPM %postun scriptlet — runs after package is removed
systemctl daemon-reload 2>/dev/null || true
