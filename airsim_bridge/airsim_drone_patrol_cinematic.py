import airsim
import time
import math
import signal
import sys

# Cinematic Path Waypoints (X, Y, Yaw)
# Smooth loop for LinkedIn demo
WAYPOINTS = [
    (0, 0, 0),
    (35, 0, 20),
    (55, 20, 60),
    (40, 45, 120),
    (10, 55, 180),
    (-20, 35, 230),
    (-25, 5, 300),
    (0, 0, 360)
]

TARGET_Z = -12.0  # Stable altitude
SPEED = 3.5       # Smooth m/s

client = airsim.MultirotorClient()

def signal_handler(sig, frame):
    print("\n[CINEMATIC] Interrupt received. Emergency landing...")
    client.landAsync().join()
    client.armDisarm(False)
    client.enableApiControl(False)
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

def run_cinematic():
    print("[CINEMATIC] Connecting to AirSim...")
    client.confirmConnection()
    client.enableApiControl(True)
    
    print("[CINEMATIC] Arming and Taking off...")
    client.armDisarm(True)
    client.takeoffAsync().join()
    
    print(f"[CINEMATIC] Climbing to {abs(TARGET_Z)}m...")
    client.moveToZAsync(TARGET_Z, 2).join()
    
    print("[CINEMATIC] Starting smooth patrol loop...")
    
    while True:
        for i, (x, y, yaw) in enumerate(WAYPOINTS):
            print(f"[CINEMATIC] Moving to Waypoint {i}: ({x}, {y}) at {yaw} degrees")
            
            # Rotate smoothly first
            client.rotateToYawAsync(yaw, margin=5).join()
            
            # Move smoothly to position
            client.moveToPositionAsync(x, y, TARGET_Z, SPEED).join()
            
            time.sleep(1)

if __name__ == "__main__":
    try:
        run_cinematic()
    except Exception as e:
        print(f"[CINEMATIC] Error: {e}")
        client.enableApiControl(False)
