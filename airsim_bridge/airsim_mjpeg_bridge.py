import time
import json
import socket
import threading
import math
import argparse
from flask import Flask, Response

try:
    import airsim
except ImportError:
    print("Error: airsim python package is missing.")
    print("Run: pip install msgpack-rpc-python airsim")
    exit(1)

try:
    import cv2
    import numpy as np
    HAS_CV2 = True
except ImportError:
    HAS_CV2 = False

app = Flask(__name__)

latest_frame = None
latest_state_json = None
latest_telemetry = {}
frames_only = False
target_width = 512
target_height = 288
frame_id = 0
frame_lock = threading.Lock()

UDP_IP = "127.0.0.1"
UDP_PORT = 5000
TARGET_FPS = 15

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def to_euler(q):
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
    global latest_frame, latest_state_json, latest_telemetry, frame_id
    
    print("Connecting to AirSim...")
    client = airsim.MultirotorClient()
    try:
        client.confirmConnection()
        print("Connected to AirSim successfully!")
    except Exception as e:
        print(f"AirSim connection failed: {e}")
        return

    cv2_warned = False

    while True:
        start_time = time.time()
        
        try:
            responses = client.simGetImages([
                airsim.ImageRequest("0", airsim.ImageType.Scene, False, True)
            ])
            
            frame_data = None
            if responses and len(responses) > 0:
                frame_data = responses[0].image_data_uint8
                
                # Resize if requested and CV2 available
                if frame_data and HAS_CV2:
                    try:
                        nparr = np.frombuffer(frame_data, np.uint8)
                        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
                        if img is not None:
                            resized = cv2.resize(img, (target_width, target_height), interpolation=cv2.INTER_AREA)
                            success, encoded = cv2.imencode(".png", resized)
                            if success:
                                frame_data = encoded.tobytes()
                    except Exception as resize_err:
                        print(f"Resize failed: {resize_err}")
                elif frame_data and not HAS_CV2 and not cv2_warned:
                    print("Warning: cv2 or numpy missing, skipping resize.")
                    cv2_warned = True

                if not frame_data:
                    print("Warning: AirSim returned empty image_data_uint8 or resize failed")
            
            state = client.getMultirotorState()
            pos = state.kinematics_estimated.position
            ori = state.kinematics_estimated.orientation
            
            roll, pitch, yaw = to_euler(ori)
            altitude = -pos.z_val
            
            with frame_lock:
                frame_id += 1
                frame_available = frame_data is not None and len(frame_data) > 0
                if frame_available:
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
                    },
                    "frame": {
                        "available": frame_available,
                        "endpoint": "http://127.0.0.1:8080/snapshot",
                        "mime": "image/png",
                        "transport": "HTTP_SNAPSHOT_PROXY"
                    }
                }
                latest_state_json = json.dumps(telemetry)
                latest_telemetry = telemetry
                
            if not frames_only:
                try:
                    sock.sendto(latest_state_json.encode('utf-8'), (UDP_IP, UDP_PORT))
                except Exception as e:
                    print(f"Warning: UDP send failed: {e}")
                
            if frame_id % 30 == 0:
                print(f"[FRAME] id={frame_id} size={target_width}x{target_height} bytes={len(frame_data) if frame_data else 0} fps_target={TARGET_FPS}")
                if not frames_only:
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

@app.route('/telemetry')
def get_telemetry():
    with frame_lock:
        return Response(json.dumps(latest_telemetry), mimetype='application/json')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--frames-only", action="store_true", help="Only serve frames/telemetry via HTTP, do not send UDP")
    parser.add_argument("--width", type=int, default=512, help="Target frame width")
    parser.add_argument("--height", type=int, default=288, help="Target frame height")
    args = parser.parse_args()
    
    frames_only = args.frames_only
    target_width = args.width
    target_height = args.height

    print(f"[BRIDGE] Target snapshot size: {target_width}x{target_height}")
    print(f"[BRIDGE] Frames only: {str(frames_only).lower()}")
    print(f"[BRIDGE] HTTP snapshot: http://127.0.0.1:8081/snapshot")
    print(f"[BRIDGE] HTTP telemetry: http://127.0.0.1:8081/telemetry")

    if frames_only:
        print("Running in --frames-only mode. UDP telemetry disabled.")

    t = threading.Thread(target=airsim_loop, daemon=True)
    t.start()
    
    print("Starting Internal MJPEG bridge on http://0.0.0.0:8081/snapshot")
    app.run(host='0.0.0.0', port=8081, threaded=True)
