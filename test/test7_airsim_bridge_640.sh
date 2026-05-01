#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=========================================================="
echo "[TEST 7] Starting AirSim Bridge (640x360)"
echo "=========================================================="

python3 airsim_bridge/airsim_mjpeg_bridge.py --frames-only --width 640 --height 360
