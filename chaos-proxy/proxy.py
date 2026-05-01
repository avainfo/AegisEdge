import socket
import random
import time
import select
import threading
import urllib.request
from collections import deque
from flask import Flask, Response

app = Flask(__name__)
last_good_frame = None
frame_lock = threading.Lock()

LISTEN_IP = "127.0.0.1"
LISTEN_PORT = 5000

TARGET_IP = "127.0.0.1"
TARGET_PORT = 6000

MODE = "STABLE"
random_mode = False

sock_in = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_in.bind((LISTEN_IP, LISTEN_PORT))
sock_in.setblocking(False)

sock_out = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

queue = deque()

def current_behavior():
    global MODE
    if random_mode:
        roll = random.random()
        if roll < 0.70: return "STABLE"
        elif roll < 0.92: return "DEGRADED"
        else: return "CUT"
    return MODE

@app.route('/snapshot')
def snapshot_proxy():
    global last_good_frame
    mode = current_behavior()
    
    if mode == "CUT":
        with frame_lock:
            if last_good_frame:
                return Response(last_good_frame, mimetype='image/png', headers={'X-Proxy-Mode': 'CUT', 'X-Video-Stale': 'true'})
            else:
                return "Video link cut", 503

    delay = 0
    if mode == "STABLE":
        delay = random.uniform(0.01, 0.04)
    elif mode == "DEGRADED":
        if random.random() < 0.15:
            return "Video congestion error", 503
        delay = random.uniform(0.3, 1.2)

    if delay > 0:
        time.sleep(delay)

    try:
        with urllib.request.urlopen("http://127.0.0.1:8081/snapshot", timeout=1.0) as response:
            frame_data = response.read()
            with frame_lock:
                last_good_frame = frame_data
            
            if mode == "DEGRADED" and random.random() < 0.3:
                return Response(frame_data, mimetype='image/png', headers={'X-Proxy-Mode': 'DEGRADED', 'X-Video-Stale': 'true'})
            
            return Response(frame_data, mimetype='image/png', headers={'X-Proxy-Mode': mode, 'X-Video-Stale': 'false'})
    except Exception as e:
        with frame_lock:
            if last_good_frame:
                return Response(last_good_frame, mimetype='image/png', headers={'X-Proxy-Mode': 'ERROR', 'X-Video-Stale': 'true'})
        return f"Upstream error: {e}", 502

def run_http_proxy():
    import logging
    log = logging.getLogger('werkzeug')
    log.setLevel(logging.ERROR)
    
    print("Starting HTTP Image Proxy on http://127.0.0.1:8080/snapshot")
    app.run(host='0.0.0.0', port=8080, threaded=True)

http_thread = threading.Thread(target=run_http_proxy, daemon=True)
http_thread.start()

print("Network proxy running")
print("UDP Listening on 127.0.0.1:5000 -> Forwarding to 127.0.0.1:6000")
print("Commands: n=stable, l=degraded, c=cut, r=random")

def process_packet(data):
    mode = current_behavior()
    if mode == "STABLE":
        delay = random.uniform(0.01, 0.04)
        drop = False
    elif mode == "DEGRADED":
        delay = random.uniform(0.3, 1.2)
        drop = random.random() < 0.25
    elif mode == "CUT":
        delay = 0
        drop = True
    else:
        delay = 0.02
        drop = False

    if drop:
        return

    send_at = time.time() + delay
    queue.append((send_at, data, mode, delay))

while True:
    now = time.time()
    readable, _, _ = select.select([sock_in], [], [], 0.01)
    for _ in readable:
        try:
            data, addr = sock_in.recvfrom(65535)
            process_packet(data)
        except:
            pass

    while queue and queue[0][0] <= now:
        send_at, data, mode, delay = queue.popleft()
        sock_out.sendto(data, (TARGET_IP, TARGET_PORT))

    # Keyboard control
    if select.select([__import__("sys").stdin], [], [], 0)[0]:
        cmd = __import__("sys").stdin.readline().strip().lower()
        if cmd == "n":
            MODE = "STABLE"
            random_mode = False
            print("MODE = STABLE")
        elif cmd == "l":
            MODE = "DEGRADED"
            random_mode = False
            print("MODE = DEGRADED")
        elif cmd == "c":
            MODE = "CUT"
            random_mode = False
            print("MODE = CUT")
        elif cmd == "r":
            random_mode = True
            print("MODE = RANDOM")
        elif cmd == "q":
            print("End of the chaos proxy")
            exit(1)
