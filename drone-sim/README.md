# AegisEdge - Drone Simulator & Detection Subsystem

This folder contains the main detection pipeline test `horizon_detection_test.cpp` and the `DetectionSystems` logic.

## Build Instructions

The build system has been unified at the repository root using CMake. Do not compile this directory manually.

From the repository root (`../`):

```bash
mkdir -p build && cd build

# Mode A: IMU + Hough Vision (Recommended for standard computers)
cmake .. -DWITH_HOUGH=ON
cmake --build .

# Mode B: IMU + Hailo NPU (Requires hailort)
cmake .. -DWITH_HAILO=ON
cmake --build .

# Mode C: IMU Only (Fallback mode)
cmake ..
cmake --build .
```

## Running the Simulation

Run the compiled executable from the root directory:

```bash
./build/horizon_detection_test [csv_path] [frames_dir] [OPTIONS]
```

### Options:
* `--udp IP:PORT` : Stream detected horizon JSON to the chaos proxy (e.g. `127.0.0.1:5000`)
* `--jsonl FILE` : Save the output line-by-line in a JSONL file
* `--no-window` : Disable OpenCV GUI display (useful for CI/CD or headless testing)

## How it works

The detection system dynamically blends IMU data with Computer Vision (or Neural Network inference) using a 1-D Kalman Filter.

To save compute power, the vision subsystem is only called when:
* The system first initializes
* A sharp maneuver is detected (Pitch/Roll changes > threshold)
* The Kalman uncertainty grows too large (time elapsed since last vision update)

When the vision is disabled, the IMU dead-reckoning provides high-frequency low-latency horizon updates, generating high compute savings (often > 70%).
