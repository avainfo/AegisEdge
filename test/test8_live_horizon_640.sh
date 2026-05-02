#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=========================================================="
echo "[TEST 8] Starting Live Horizon Bridge (640x360 DEBUG)"
echo "=========================================================="

if [ ! -f "./build/live_horizon_bridge" ]; then
    echo "[ERROR] live_horizon_bridge binary missing."
    echo "Please run ./test/test0_build.sh first."
    exit 1
fi

echo "WATCH FOR:"
echo "  - size=640x360"
echo "  - fps>=15"
echo "=========================================================="

./build/live_horizon_bridge --debug-window
