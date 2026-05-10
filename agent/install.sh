#!/usr/bin/env bash
# install.sh - install the Veyon Policy Agent system-wide.
#
# Usage:
#   sudo ./install.sh
#
# This script:
#   1. Copies the binary to /usr/local/bin
#   2. Installs the systemd unit
#   3. Creates /etc/veyon-policy-agent/ with example config (if missing)
#   4. Reloads systemd
#
# Does NOT enable or start the service - you must do that manually
# after editing /etc/veyon-policy-agent/config.conf.

set -euo pipefail

if [[ "$EUID" -ne 0 ]]; then
    echo "This script must be run with sudo." >&2
    exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="$SCRIPT_DIR/../build/agent"

if [[ ! -x "$BUILD_DIR/veyon-policy-agent" ]]; then
    echo "Build artifact not found: $BUILD_DIR/veyon-policy-agent"
    echo "Run cmake + make first. From the repo root:"
    echo "  mkdir -p build && cd build && cmake .. && make veyon-policy-agent"
    exit 1
fi

echo "[1/4] Installing binary to /usr/local/bin"
install -m 0755 "$BUILD_DIR/veyon-policy-agent" /usr/local/bin/veyon-policy-agent

echo "[2/4] Installing systemd unit"
install -m 0644 "$SCRIPT_DIR/veyon-policy-agent.service.in" \
    /etc/systemd/system/veyon-policy-agent.service

echo "[3/4] Creating /etc/veyon-policy-agent (if missing)"
mkdir -p /etc/veyon-policy-agent
chmod 750 /etc/veyon-policy-agent
if [[ ! -f /etc/veyon-policy-agent/config.conf ]]; then
    install -m 0640 "$SCRIPT_DIR/config.example.conf" \
        /etc/veyon-policy-agent/config.conf
    echo "      Created /etc/veyon-policy-agent/config.conf - edit it before starting!"
else
    echo "      Existing config preserved at /etc/veyon-policy-agent/config.conf"
fi

echo "[4/4] Reloading systemd"
systemctl daemon-reload

cat <<MSG

Installation complete.

Next steps:
  1. Edit /etc/veyon-policy-agent/config.conf
     (set server_url and admin_token)
  2. sudo systemctl enable --now veyon-policy-agent
  3. sudo systemctl status veyon-policy-agent
  4. sudo journalctl -u veyon-policy-agent -f

MSG
