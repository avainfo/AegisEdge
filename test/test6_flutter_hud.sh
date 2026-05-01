#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=========================================================="
echo "[TEST 6] Starting Flutter HUD"
echo "=========================================================="

cd aegis_edge_hud
echo "[1/2] Fetching dependencies..."
flutter pub get

echo "[2/2] Running on Linux desktop..."
flutter run -d linux
