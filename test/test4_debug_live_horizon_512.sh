#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=========================================================="
echo "[DEBUG] Starting Live Horizon Bridge (512x288 DEBUG MODE)"
echo "=========================================================="

if [ ! -f "./build/live_horizon_bridge" ]; then
    echo "[ERROR] live_horizon_bridge binary missing."
    echo "Please run ./test/test0_build.sh first."
    exit 1
fi

echo "WARNING: Debug mode is CPU intensive (OpenCV imshow)."
echo "Do not use this for the final demo."
echo "----------------------------------------------------------"
echo "WATCH FOR:"
echo "  - size=512x288"
echo "  - fps>=15"
echo "  - angle vs expected"
echo "=========================================================="

./build/live_horizon_bridge --debug-window
