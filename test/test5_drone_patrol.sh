#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=========================================================="
echo "[TEST 5] Starting Drone Patrol (Automatic Movement)"
echo "=========================================================="

python3 airsim_bridge/airsim_drone_patrol.py
