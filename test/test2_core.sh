#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=========================================================="
echo "[TEST 2] Starting Aegis Core"
echo "=========================================================="

if [ ! -f "./core/build/aegis_core" ]; then
    echo "[ERROR] Core binary missing."
    echo "Please run ./test/test0_build.sh first."
    exit 1
fi

./core/build/aegis_core
