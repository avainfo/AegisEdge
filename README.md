# Aegis Edge

> Aegis Edge is a resilient tactical HUD that maintains situational awareness under degraded drone telemetry and vision conditions.

---

## Demo

A short demo of the project is available here:

[Watch the Aegis Edge demo on LinkedIn](https://www.linkedin.com/feed/update/urn:li:activity:7457829124796129280/)

The demo shows the HUD reacting to live telemetry, degraded signal conditions, full signal loss, and recovery.

---

## Architecture

```
[Telemetry/Detection source]
        ↓ UDP 5000
[chaos-proxy/proxy.py]
        ↓ UDP 6000
[core/build/aegis_core]       ← computes NORMAL / DEGRADED / LOST
        ↓ UDP 5005
[Flutter HUD]                  ← displays live state, horizon, telemetry
```

### Ports
| Port | Role |
|------|------|
| 5000 | Sender (fake telemetry or detection test) → Chaos proxy |
| 6000 | Chaos proxy → C++ Core backend |
| 5005 | C++ Core → Flutter HUD |

---

## Running the Demo

### Mode A — Fake Telemetry (quick demo, no camera needed)

```bash
# Terminal 1 — Chaos proxy
python3 chaos-proxy/proxy.py

# Terminal 2 — C++ Core
cmake -S core -B core/build
cmake --build core/build
./core/build/aegis_core

# Terminal 3 — Fake telemetry sender (~30 Hz)
python3 tools/send_fake_telemetry.py

# Terminal 4 — Flutter HUD
cd aegis_edge_hud
flutter run -d linux
```

### Mode B — Detection Pipeline (with AirSim data or real frames)

```bash
# Terminal 1 — Chaos proxy
python3 chaos-proxy/proxy.py

# Terminal 2 — C++ Core
cmake -S core -B core/build
cmake --build core/build
./core/build/aegis_core

# Terminal 3 — Detection test (WITH_HOUGH mode)
mkdir -p build
cmake -B build -DWITH_HOUGH=ON
cmake --build build
./build/horizon_detection_test --udp 127.0.0.1:5000 --no-window

# Terminal 4 — Flutter HUD
cd aegis_edge_hud
flutter run -d linux
```

---

## Quick Validation

```bash
# Build core
cmake -S core -B core/build
cmake --build core/build

# Build detection in Hough mode
cmake -B build -DWITH_HOUGH=ON
cmake --build build

# Generate JSONL without UDP
./build/horizon_detection_test --no-window --jsonl detected_horizon_output.jsonl

# Check output exists
ls -lh detected_horizon_output.jsonl
```

* Detection build requires OpenCV (`sudo apt-get install libopencv-dev`).
* Hailo mode is optional and requires hailort.
* Default demo mode is WITH_HOUGH, not Hailo.

---

## Chaos Proxy Controls

While the proxy is running, press a key to change the network mode:

| Key | Mode | Effect |
|-----|------|--------|
| `n` | Stable | Packets forwarded normally |
| `l` | Degraded | Packets delayed (high latency) |
| `c` | Cut | All packets dropped (LOST) |
| `r` | Random | Random packet loss |
| `q` | Quit | Stop the proxy |

---

## Building the Detection Test

From the repository root:

```bash
mkdir -p build && cd build

# IMU + Hough (recommended for demo without Hailo hardware)
cmake .. -DWITH_HOUGH=ON
cmake --build .

# IMU only (no vision)
cmake ..
cmake --build .

# IMU + Hailo NPU (requires hailort installed)
cmake .. -DWITH_HAILO=ON
cmake --build .
```

---

## Detection Test Options

```
./build/horizon_detection_test [csv_path] [frames_dir] [OPTIONS]

OPTIONS:
  --udp IP:PORT       Stream JSON packets to chaos proxy (e.g. 127.0.0.1:5000)
  --jsonl FILE        Write all frames as JSONL to FILE
  --no-window         Disable OpenCV display window
  --window 0|1        Explicitly set window on/off
```

### Example outputs

**Detection running (with UDP):**
```
[AegisEdge] Mode: IMU + Hough (Classic CV)
[AegisEdge] UDP -> 127.0.0.1:5000
t=1777083609789 | pitch=-1.4 | roll=2.1 | offset=8.3 | angle=2.0 | conf=0.78 [VISION]
t=1777083609822 | pitch=-1.5 | roll=2.3 | offset=8.1 | angle=2.1 | conf=0.75 [VISION]
```

**JSONL output format per line:**
```json
{
  "timestamp_ms": 1777083609789,
  "drone_id": "DRONE_01",
  "position": { "x": 12.4, "y": 8.7 },
  "roll": 2.1, "pitch": -1.4, "yaw": 87.0, "altitude": 42.5,
  "horizon": {
    "detected": true,
    "type": "POLYLINE",
    "points": [{"x":0.05,"y":0.42},{"x":0.50,"y":0.41},{"x":0.95,"y":0.44}],
    "ground_side": "BELOW",
    "confidence": 0.78
  }
}
```

---

## Project Structure

```
AegisEdge/
├── CMakeLists.txt              ← Detection test build (root)
├── core/                       ← C++ Core backend (state machine + UDP relay)
│   ├── CMakeLists.txt
│   ├── include/
│   └── src/
├── chaos-proxy/
│   ├── proxy.py                ← Chaos proxy (n/l/c/r/q controls)
│   └── README.md
├── drone-sim/
│   └── horizon_detection_test.cpp  ← Main detection test binary
├── DetectionSystems/
│   ├── horizon.hpp             ← OptimizedHorizonDetector interface
│   └── ohd.cpp                 ← Detector implementation (IMU/Hough/Hailo)
├── aegis_edge_hud/             ← Flutter HUD application
│   └── lib/
├── tools/
│   └── send_fake_telemetry.py  ← Fake telemetry sender (no camera needed)
├── Assets/                     ← AirSim logs and frames
└── info/                       ← JSON format templates
```

---

## Link State Logic (C++ Core)

| Condition | State |
|-----------|-------|
| Latency < 300 ms | NORMAL (green) |
| Latency 300–2000 ms | DEGRADED (amber) |
| No packet for > 2000 ms | LOST (red) |

When LOST: the HUD freezes the last valid telemetry and shows a stale overlay. The horizon polyline remains visible from last known data.
