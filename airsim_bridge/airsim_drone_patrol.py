import airsim
import time
import sys
import signal

def main():
    client = airsim.MultirotorClient()
    try:
        client.confirmConnection()
        print("[DRONE] Connected to AirSim")
    except Exception as e:
        print(f"[DRONE] Connection failed: {e}")
        sys.exit(1)

    signal.signal(signal.SIGINT, signal_handler)

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

    target_z = -12
    print(f"[DRONE] Climbing to {abs(target_z)}m...")
    client.moveToZAsync(target_z, 3).join()

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
                
                move_task = client.moveToPositionAsync(x, y, target_z, 5)
                yaw_task = client.rotateToYawAsync(yaw, 10)
                
                move_task.join()
                yaw_task.join()
                
                time.sleep(1)
                
    except Exception as e:
        print(f"[DRONE] Error in patrol: {e}")
        signal_handler(None, None)

if __name__ == "__main__":
    main()
