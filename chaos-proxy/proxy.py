import socket
import random
import time
import select
from collections import deque

LISTEN_IP = "127.0.0.1"
LISTEN_PORT = 5000

TARGET_IP = "127.0.0.1"
TARGET_PORT = 6000

MODE = "STABLE"

sock_in = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_in.bind((LISTEN_IP, LISTEN_PORT))
sock_in.setblocking(False)

sock_out = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

queue = deque()

print("Network proxy running")
print("Listening on 127.0.0.1:5000")
print("Forwarding to 127.0.0.1:6000")
print("Commands: n=stable, l=degraded, c=cut, r=random")

random_mode = False

def current_behavior():
	global MODE

	if random_mode:
		roll = random.random()

		if roll < 0.70:
			return "STABLE"
		elif roll < 0.92:
			return "DEGRADED"
		else:
			return "CUT"

	return MODE

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
		print(f"mode={mode} dropped=true")
		return

	send_at = time.time() + delay
	queue.append((send_at, data, mode, delay))

while True:
	now = time.time()

	readable, _, _ = select.select([sock_in], [], [], 0.01)

	for _ in readable:
		data, addr = sock_in.recvfrom(65535)
		process_packet(data)

	while queue and queue[0][0] <= now:
		send_at, data, mode, delay = queue.popleft()
		sock_out.sendto(data, (TARGET_IP, TARGET_PORT))
		print(f"mode={mode} forwarded=true delay_ms={int(delay * 1000)} size={len(data)}")

	# contrôle clavier simple
	if select.select([__import__("sys").stdin], [], [], 0)[0]:
		cmd = input().strip().lower()

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

