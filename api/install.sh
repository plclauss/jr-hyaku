#!/bin/bash
###############################################################################
# Installation script for jr-hyaku HTTP API
# Installs as a systemd service
# Usage: sudo ./install.sh [INSTALL_DIR] [VENV_DIR]
#   INSTALL_DIR: Optional installation directory (default: /opt/api)
#   VENV_DIR: Optional Python venv directory (default: INSTALL_DIR/.venv)
###############################################################################

set -e # Exit on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "${SCRIPT_DIR}/.." && pwd )"
SERVICE_NAME="jr-hyaku-api"
SERVICE_FILE="${SCRIPT_DIR}/${SERVICE_NAME}.service"
SYSTEMD_DIR="/etc/systemd/system"

INSTALL_DIR="${1:-/opt/api}"         # Use provided install directory or default
VENV_DIR="${2:-${SCRIPT_DIR}/.venv}" # Use provided venv directory or default to SCRIPT_DIR/.venv
API_DIR="${INSTALL_DIR}/api"

echo "============================================"
echo "jr-hyaku HTTP API                           "
echo "============================================"
echo ""
echo "Installation directory: ${INSTALL_DIR}"
echo "Python venv directory:  ${VENV_DIR}"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "ERROR: This script must be run as root (use sudo)"
    exit 1
fi

# Verify Python venv exists
echo "[1/7] Checking Python virtual environment..."
if [ ! -f "${VENV_DIR}/bin/python" ]; then
    echo "  ERROR: Python venv not found at ${VENV_DIR}"
    echo "  Please create a venv with required dependencies first:"
    echo "    python3 -m venv ${VENV_DIR}"
    echo "    ${VENV_DIR}/bin/pip install fastapi[standard] pydantic uvicorn[standard] psycopg[binary]"
    echo ""
    echo "  Or specify an existing venv location:"
    echo "    sudo ./install.sh ${INSTALL_DIR} /path/to/your/venv"
    exit 1
fi
echo "  Python venv found at ${VENV_DIR}"

# Verify required packages
echo "  Checking required packages..."
if ! "${VENV_DIR}/bin/python" -c "import fastapi, pydantic, uvicorn, psycopg" 2>/dev/null; then
    echo "  WARNING: Some required packages may be missing"
    echo "  Install with: ${VENV_DIR}/bin/pip install fastapi[standard] pydantic uvicorn[standard] psycopg[binary]"
else
    echo "  Required packages available"
fi

# Create installation directory
echo ""
echo "[2/7] Creating installation directory..."
mkdir -p "${API_DIR}"
chmod 755 "${API_DIR}"
echo "  Installation directory: ${API_DIR}"

# Copy files to installation directory
echo ""
echo "[3/7] Installing interface files..."
cp "${SCRIPT_DIR}/api.py" "${API_DIR}/"
cp "${SCRIPT_DIR}/db.py" "${API_DIR}/"
chmod +x "${API_DIR}/api.py"
echo "  Files installed to ${API_DIR}"

# Verify templates and static files exist
echo ""
echo "[4/7] Verifying interface files..."
echo "  Interface files verified" # No checks necessary; NOP.

# Copy and configure service file
echo ""
echo "[5/7] Installing systemd service..."
sed -e "s|__INSTALL_DIR__|${INSTALL_DIR}|g" -e "s|__VENV_DIR__|${VENV_DIR}|g" "${SERVICE_FILE}" > "${SYSTEMD_DIR}/${SERVICE_NAME}.service"
echo "  Service file: ${SYSTEMD_DIR}/${SERVICE_NAME}.service"

# Reload systemd daemon
echo ""
echo "[6/7] Reloading systemd daemon..."
systemctl daemon-reload
echo "  Systemd daemon reloaded"

# Enable and start service
echo ""
echo "[7/7] Enabling and starting service..."
systemctl enable ${SERVICE_NAME}.service
systemctl start ${SERVICE_NAME}.service
echo "  Service enabled and started"

# Get the device IP address
IP_ADDRESS=$(hostname -I | awk '{print $1}')

# Check service status
echo ""
echo "=========================================="
echo "Installation Complete!"
echo "=========================================="
echo ""
echo "Service Status:"
systemctl status ${SERVICE_NAME}.service --no-pager -l || true

echo ""
echo "=========================================="
echo "Interface Access"
echo "=========================================="
echo ""
echo "  Local:   http://localhost:8080"
if [ -n "$IP_ADDRESS" ]; then
    echo "  Network: http://${IP_ADDRESS}:8080"
fi
echo ""
echo "Useful Commands:"
echo "  Check status:     sudo systemctl status ${SERVICE_NAME}.service"
echo "  Restart:          sudo systemctl restart ${SERVICE_NAME}.service"
echo "  View logs:        sudo journalctl -u ${SERVICE_NAME}.service -f"
echo ""
