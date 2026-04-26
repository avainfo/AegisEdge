# Aegis Edge HUD

The Flutter tactical Heads-Up Display (HUD) for the Aegis Edge project.

This application is designed to render critical telemetry and real-time Artificial Horizon data, seamlessly handling missing or degraded UDP packets through an intermediate Chaos proxy layer.

## Requirements

* Flutter 3.x
* Linux Desktop (or other supported platforms, though currently optimized for Linux)

## Running the Application

```bash
cd aegis_edge_hud
flutter pub get
flutter run -d linux
```

## Architecture

The HUD expects incoming JSON packets strictly conforming to the `DroneState` data model, broadcasted by the `aegis_core` backend on **UDP port 5005**.

### Key Components:
- **`UdpService`**: Asynchronous UDP listener on port 5005 that decodes JSON telemetry.
- **`HudController`**: A state manager tracking the current `LinkState` (NORMAL, DEGRADED, LOST) based on packet arrival rates and proxy flags.
- **`HudScreen`**: Decoupled, heavily-optimized UI layers designed to render telemetry and the Artificial Horizon at 60 FPS without triggering full-screen repaints.

If UDP packets stop arriving, the HUD automatically degrades its visual elements to indicate a "LOST" state, keeping the last-known telemetry data visible under a stale overlay.
