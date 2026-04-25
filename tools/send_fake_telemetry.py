import socket
import json
import time
import math

UDP_IP = "127.0.0.1"
UDP_PORT = 5000

print(f"Sending fake telemetry to {UDP_IP}:{UDP_PORT}")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

frame_id = 0
start_time = time.time()

while True:
    current_time_ms = int(time.time() * 1000)
    
    # Simulate some simple movement
    t = time.time() - start_time
    roll = math.sin(t) * 15.0
    pitch = math.cos(t * 0.5) * 10.0
    yaw = (t * 5.0) % 360.0
    
    # Artificial horizon points moving slightly
    y_offset = pitch / 40.0
    rot = roll * math.pi / 180.0
    
    points = [
        { "x": 0.05, "y": 0.5 + y_offset - math.sin(rot)*0.4 },
        { "x": 0.50, "y": 0.5 + y_offset },
        { "x": 0.95, "y": 0.5 + y_offset + math.sin(rot)*0.4 }
    ]

    data = {
        "frame_id": frame_id,
        "timestamp_ms": current_time_ms,
        "drone_id": "DRONE_01",
        "position": { "x": 12.4 + math.sin(t*0.1)*5, "y": 8.7 + math.cos(t*0.1)*5 },
        "roll": roll,
        "pitch": pitch,
        "yaw": yaw,
        "altitude": 42.5 + math.sin(t*0.2)*2,
        "image": {
            "width": 1920,
            "height": 1080
        },
        "horizon": {
            "detected": True,
            "type": "POLYLINE",
            "points": points,
            "ground_side": "BELOW",
            "confidence": 0.85 + math.sin(t)*0.1
        }
    }
    
    try:
        sock.sendto(json.dumps(data).encode('utf-8'), (UDP_IP, UDP_PORT))
    except Exception as e:
        print(f"Error sending: {e}")
        
    frame_id += 1
    
    # ~30 Hz
    time.sleep(1.0 / 30.0)
