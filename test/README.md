# AegisEdge Test Launchers

This directory contains shell scripts to launch each component of the AegisEdge pipeline in separate terminals.

## 🚀 Quick Start Order

### Step 0: Initial Build
Compile native C++ components and check Python deps:
```bash
./test/test0_build.sh
```

### Full Pipeline Launch
Open one terminal for each of the following scripts (in order):

1. **Terminal 1: Network Proxy**
   `./test/test1_chaos_proxy.sh` (Controls NORMAL / DEGRADED / LOST states)
2. **Terminal 2: Aegis Core**
   `./test/test2_core.sh` (Central telemetry hub)
3. **Terminal 3: AirSim Bridge**
   `./test/test3_airsim_bridge_512.sh` (Pulls images from AirSim)
4. **Terminal 4: Horizon Detection**
   `./test/test4_live_horizon_512.sh` (Processes images + IMU)
5. **Terminal 5: Drone Patrol**
   `./test/test5_drone_patrol.sh` (Automates AirSim movement)
6. **Terminal 6: Flutter HUD**
   `./test/test6_flutter_hud.sh` (Visual Dashboard)

---

## 📊 Benchmark: High Resolution
To test if the system can handle 640x360 while maintaining >= 15 FPS:
1. Stop Terminal 3 and 4.
2. Run `./test/test7_airsim_bridge_640.sh` in Terminal 3.
3. Run `./test/test8_live_horizon_640.sh` in Terminal 4.

If FPS drops below 15, revert to the 512x288 scripts.

---

## ✅ Validation Checklist
- [ ] AirSim bridge shows frame size 512x288 or 640x360.
- [ ] `live_horizon_bridge` shows `fps >= 15`.
- [ ] Flutter HUD displays live video and telemetry.
- [ ] Drone is moving in AirSim.
- [ ] Horizon line appears and follows drone attitude.
- [ ] Chaos Proxy keys `n`/`l`/`c` correctly affect HUD connection status.

## 🛠 Setup
Ensure all scripts are executable:
```bash
chmod +x test/*.sh
```
