import time
import json
import socket
import threading
import math
from flask import Flask, Response

try:
    import airsim
except ImportError:
    print("Error: airsim python package is missing.")
    print("Run: pip install msgpack-rpc-python airsim")
    exit(1)

app = Flask(__name__)

# Global variables for the latest frame and state
latest_frame = None
latest_state_json = None
frame_id = 0
frame_lock = threading.Lock()

UDP_IP = "127.0.0.1"
UDP_PORT = 5000  # Target port (proxy or core)
TARGET_FPS = 15

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def to_euler(q):
    """Convert AirSim quaternion to roll, pitch, yaw in degrees"""
    w, x, y, z = q.w_val, q.x_val, q.y_val, q.z_val
    t0 = +2.0 * (w * x + y * z)
    t1 = +1.0 - 2.0 * (x * x + y * y)
    roll = math.degrees(math.atan2(t0, t1))

    t2 = +2.0 * (w * y - z * x)
    t2 = +1.0 if t2 > +1.0 else t2
    t2 = -1.0 if t2 < -1.0 else t2
    pitch = math.degrees(math.asin(t2))

    t3 = +2.0 * (w * z + x * y)
    t4 = +1.0 - 2.0 * (y * y + z * z)
    yaw = math.degrees(math.atan2(t3, t4))

    return roll, pitch, yaw

def generate_fake_horizon(roll, pitch):
    """Generate fake horizon points between 0.0 and 1.0 based on drone attitude"""
    # Simply keep points at y=0.5 but adjust based on pitch/roll
    # This is a very rough simulation just for hackathon demo
    pitch_offset = -pitch / 90.0 * 0.5 
    roll_rad = math.radians(roll)
    
    y_center = 0.5 + pitch_offset
    
    # Calculate left, center, right points
    x_left, x_center, x_right = 0.1, 0.5, 0.9
    
    # Simple roll rotation around center
    y_left = y_center - math.tan(roll_rad) * (x_center - x_left)
    y_right = y_center + math.tan(roll_rad) * (x_right - x_center)
    
    # Clamp
    y_left = max(0.0, min(1.0, y_left))
    y_center = max(0.0, min(1.0, y_center))
    y_right = max(0.0, min(1.0, y_right))
    
    return [
        {"x": x_left, "y": y_left},
        {"x": x_center, "y": y_center},
        {"x": x_right, "y": y_right}
    ]

def airsim_loop():
    global latest_frame, latest_state_json, frame_id
    
    print("Connecting to AirSim...")
    client = airsim.MultirotorClient()
    try:
        client.confirmConnection()
        print("Connected to AirSim successfully!")
    except Exception as e:
        print(f"AirSim connection failed: {e}")
        return

    while True:
        start_time = time.time()
        
        try:
            # Get Image
            responses = client.simGetImages([
                airsim.ImageRequest("0", airsim.ImageType.Scene, False, True)
            ])
            if responses and len(responses) > 0:
                frame_data = responses[0].image_data_uint8
                if not frame_data:
                    print("Warning: AirSim returned empty image_data_uint8")
            else:
                frame_data = None
                print("Warning: AirSim returned no response for simGetImages")
                
            # Get Kinematics
            state = client.getMultirotorState()
            pos = state.kinematics_estimated.position
            ori = state.kinematics_estimated.orientation
            
            roll, pitch, yaw = to_euler(ori)
            altitude = -pos.z_val # NED to altitude
            
            with frame_lock:
                frame_id += 1
                if frame_data is not None:
                    latest_frame = frame_data
                
                timestamp_ms = int(time.time() * 1000)
                
                horizon_pts = generate_fake_horizon(roll, pitch)
                
                telemetry = {
                    "frame_id": frame_id,
                    "timestamp_ms": timestamp_ms,
                    "position": {"x": pos.x_val, "y": pos.y_val},
                    "roll": roll,
                    "pitch": pitch,
                    "yaw": yaw,
                    "altitude": altitude,
                    "horizon": {
                        "detected": True,
                        "confidence": 0.85,
                        "points": horizon_pts
                    }
                }
                latest_state_json = json.dumps(telemetry)
                
            # Send UDP Telemetry
            try:
                sock.sendto(latest_state_json.encode('utf-8'), (UDP_IP, UDP_PORT))
            except Exception as e:
                print(f"Warning: UDP send failed: {e}")
                
            if frame_id % 30 == 0:
                print(f"[FRAME] id={frame_id} bytes={len(frame_data) if frame_data else 0} ts={timestamp_ms}")
                print(f"[UDP] sent telemetry to {UDP_IP}:{UDP_PORT}")
            
        except Exception as e:
            print(f"Error in AirSim loop: {e}")
            time.sleep(1)
            
        elapsed = time.time() - start_time
        sleep_time = max(0.0, (1.0 / TARGET_FPS) - elapsed)
        time.sleep(sleep_time)

def generate_mjpeg():
    while True:
        with frame_lock:
            frame = latest_frame
        
        if frame is not None:
            yield (b'--frame\r\n'
                   b'Content-Type: image/png\r\n\r\n' + frame + b'\r\n')
        
        time.sleep(1.0 / TARGET_FPS)

@app.route('/video')
def video_feed():
    return Response(generate_mjpeg(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/snapshot')
def snapshot():
    with frame_lock:
        frame = latest_frame
    if frame is None:
        return "No valid frame yet", 404
    return Response(frame, mimetype='image/png')

if __name__ == '__main__':
    t = threading.Thread(target=airsim_loop, daemon=True)
    t.start()
    
    print("Starting MJPEG bridge on http://0.0.0.0:8080/video")
    app.run(host='0.0.0.0', port=8080, threaded=True)
