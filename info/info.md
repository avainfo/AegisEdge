# Info JSON Templates

This folder contains JSON templates for the horizon detection output.

## Files

- `json_awaited_hf.json`
  Example when the horizon is detected.

- `json_awaited_hnf.json`
  Example when the horizon is not detected.

## JSON Structure

Each file follows this structure:

```json
{
  "frame_id": 123,
  "timestamp_ms": 6334,
  "image": {
    "width": 1920,
    "height": 1080
  },
  "horizon": {
    "detected": true,
    "type": "POLYLINE",
    "points": [],
    "ground_side": "BELOW",
    "confidence": 0.76
  }
}
````

## Horizon Found

When the horizon is detected:

```json
"horizon": {
  "detected": true,
  "type": "POLYLINE",
  "points": [
    { "x": 0.05, "y": 0.42 },
    { "x": 0.50, "y": 0.41 },
    { "x": 0.95, "y": 0.44 }
  ],
  "ground_side": "BELOW",
  "confidence": 0.76
}
```

## Horizon Not Found

When no horizon is detected:

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
* `confidence` is between `0.0` and `1.0`.
* The backend must handle both detected and non-detected horizon cases.

