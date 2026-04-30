# This script controls the simulated drone motion. 
# It is intentionally separate from the AirSim bridge, which only captures frames and telemetry.

import airsim
import time
import sys
import signal

def main():
    # Connect to AirSim
    client = airsim.MultirotorClient()
    try:
        client.confirmConnection()
        print("[DRONE] Connected to AirSim")
    except Exception as e:
        print(f"[DRONE] Connection failed: {e}")
        sys.exit(1)

    # Clean exit handler
    def signal_handler(sig, frame):
        print("\n[DRONE] Interrupt received. Landing safely...")
        client.moveByVelocityAsync(0, 0, 0, 1).join()
        print("[DRONE] Landing...")
        client.landAsync().join()
        print("[DRONE] Disarming...")
        client.armDisarm(False)
        client.enableApiControl(False)
        print("[DRONE] Done. Safe flight!")
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)

    print("[DRONE] Enabling API control...")
    client.enableApiControl(True)
    
    print("[DRONE] Arming...")
    client.armDisarm(True)

    print("[DRONE] Taking off...")
    client.takeoffAsync().join()

    # Initial climb
    target_z = -12 # 12 meters altitude (NED coordinates)
    print(f"[DRONE] Climbing to {abs(target_z)}m...")
    client.moveToZAsync(target_z, 3).join()

    # Waypoints: (x, y, yaw)
    waypoints = [
        (0, 0, 0),
        (25, 0, 45),
        (25, 25, 135),
        (0, 25, 225),
        (0, 0, 315)
    ]

    print("[DRONE] Starting patrol loop. Press Ctrl+C to land.")
    
    try:
        while True:
            for x, y, yaw in waypoints:
                print(f"[DRONE] Moving to waypoint: x={x}, y={y}, yaw={yaw}")
                
                # Move to position and rotate yaw simultaneously
                move_task = client.moveToPositionAsync(x, y, target_z, 5)
                yaw_task = client.rotateToYawAsync(yaw, 10)
                
                # Wait for movement to finish
                move_task.join()
                yaw_task.join()
                
                time.sleep(1) # Small pause at waypoint
                
    except Exception as e:
        print(f"[DRONE] Error in patrol: {e}")
        signal_handler(None, None)

if __name__ == "__main__":
    main()
