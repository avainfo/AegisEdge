#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=========================================================="
echo "[TEST 3] Starting AirSim Bridge (512x288)"
echo "=========================================================="
echo "Endpoints available:"
echo "  Snapshot:  http://127.0.0.1:8081/snapshot"
echo "  Telemetry: http://127.0.0.1:8081/telemetry"
echo "=========================================================="

python3 airsim_bridge/airsim_mjpeg_bridge.py --frames-only --width 512 --height 288
