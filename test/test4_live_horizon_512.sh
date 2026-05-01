#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=========================================================="
echo "[TEST 4] Starting Live Horizon Bridge (512x288 DEBUG)"
echo "=========================================================="

if [ ! -f "./build/live_horizon_bridge" ]; then
    echo "[ERROR] live_horizon_bridge binary missing."
    echo "Please run ./test/test0_build.sh first."
    exit 1
fi

echo "WATCH FOR:"
echo "  - size=512x288"
echo "  - fps>=15"
echo "  - source=VISION_HOUGH (when reliable)"
echo "  - source=IMU_ESTIMATED (fallback)"
echo "=========================================================="

./build/live_horizon_bridge --debug-window
