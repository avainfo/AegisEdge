# Info JSON Templates

This folder contains JSON templates for the horizon detection output.

## JSON Structure

Each file follows this structure:

```json
{
  "frame_id": 123,
  "timestamp_ms": 6334,
  "drone_id": "DRONE_01",
  "position": {
    "x": 12.4,
    "y": 8.7
  },
  "roll": 23.6,
  "pitch": 8.8,
  "yaw": 0.0,
  "altitude": 42.5,
  "image": {
    "width": 1920,
    "height": 1080
  },
  "horizon": {
    "detected": true,
    "type": "POLYLINE",
    "points": [
      { "x": 0.05, "y": 0.42 },
      { "x": 0.50, "y": 0.41 },
      { "x": 0.95, "y": 0.44 }
    ],
    "ground_side": "BELOW",
    "confidence": 0.76,
    "estimated": false
  }
}
```

## Horizon States

### 1. Horizon Found (Vision-confirmed)

When the horizon is successfully detected via Hough or Hailo:

```json
"horizon": {
  "detected": true,
  "type": "POLYLINE",
  "points": [...],
  "ground_side": "BELOW",
  "confidence": 0.76
}
```
*(Confidence usually ranges between 0.65 and 0.90. `estimated` field is omitted or false).*

### 2. Horizon Estimated (IMU Fallback)

When the vision gate fails or isn't called, the system falls back to IMU predictions:

```json
"horizon": {
  "detected": true,
  "type": "POLYLINE",
  "points": [...],
  "ground_side": "BELOW",
  "confidence": 0.55,
  "estimated": true
}
```
*(Confidence is capped lower, typically around 0.55, to signal that this is an estimation).*

### 3. Horizon Not Found

When no horizon is detected and no valid IMU fallback is available:

```json
"horizon": {
  "detected": false,
  "type": "NONE",
  "points": [],
  "ground_side": "UNKNOWN",
  "confidence": 0.0
}
```

## Notes

* `points` use normalized coordinates between `0.0` and `1.0`.
* `confidence` is clamped between `0.0` and `1.0`.
* The `aegis_core` backend parses these values and enriches the packet with `link_state`, `stale`, and `latency_ms` properties before forwarding to the Flutter HUD.
