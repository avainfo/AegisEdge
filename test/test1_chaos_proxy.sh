#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=========================================================="
echo "[TEST 1] Starting Chaos Proxy"
echo "=========================================================="
echo "Use keys inside this terminal to simulate network conditions:"
echo "  n = NORMAL (no lag)"
echo "  l = LAG / DEGRADED (simulates jitter/latency)"
echo "  c = CUT / LOST (simulates connection loss)"
echo "  r = RANDOM (fluctuating conditions)"
echo "=========================================================="

python3 chaos-proxy/proxy.py
