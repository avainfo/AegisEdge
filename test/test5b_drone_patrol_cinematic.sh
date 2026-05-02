#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=========================================================="
echo "[TEST 5b] Starting Cinematic Drone Patrol"
echo "=========================================================="
echo "NOTE: This mode is optimized for LinkedIn/Project recording."
echo "      - Slow, smooth movements"
echo "      - Stable altitude"
echo "      - Gradual yaw changes"
echo "=========================================================="

python3 airsim_bridge/airsim_drone_patrol_cinematic.py
