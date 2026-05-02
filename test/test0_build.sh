#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=========================================================="
echo "[TEST 0] Building AegisEdge Native Components"
echo "=========================================================="

# Check Python dependencies
echo "[1/3] Checking Python dependencies..."
python3 -c "import cv2, numpy, airsim; print('[OK] python deps: cv2, numpy, airsim')"

# Build live_horizon_bridge
echo "[2/3] Building live_horizon_bridge (HOUGH=ON)..."
cmake -B build -DWITH_HOUGH=ON
cmake --build build

# Build Aegis Core
echo "[3/3] Building Aegis Core..."
cmake -S core -B core/build
cmake --build core/build

echo "=========================================================="
echo "[OK] Build complete. Ready to launch tests."
echo "=========================================================="
