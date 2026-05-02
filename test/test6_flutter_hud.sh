#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=========================================================="
echo "[TEST 6] Starting Flutter HUD (Linux release)"
echo "=========================================================="

cd aegis_edge_hud
echo "[1/3] Fetching dependencies..."
flutter pub get

echo "[2/3] Building Linux release binary..."
flutter build linux --release

if [ ! -x "./build/linux/x64/release/bundle/aegis_edge_hud" ]; then
    echo "[ERROR] Flutter release binary missing."
    exit 1
fi

echo "[3/3] Running release bundle..."
./build/linux/x64/release/bundle/aegis_edge_hud
