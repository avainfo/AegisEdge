# UDP Chaos Proxy

A lightweight UDP proxy used to simulate unstable network conditions between a sender and a receiver.

The proxy listens for incoming UDP packets on `127.0.0.1:5000`, applies simulated network behavior, then forwards the packets to `127.0.0.1:6000`.

It supports several network modes:

- Stable connection
- Degraded connection with latency and packet loss
- Full signal cut
- Random mode switching between the previous states

## Purpose

This tool is designed to test how a real-time application behaves under imperfect network conditions.

It can be used for:

- Tactical HUD prototypes
- Drone telemetry simulations
- Flutter applications receiving UDP data
- Real-time monitoring interfaces
- Communication robustness testing
- Signal loss demonstrations

The goal is to verify whether the receiving application correctly handles:

- Network latency
- Packet loss
- Full connection loss
- Unstable signal quality
- Recovery after degradation

## Architecture

```txt
UDP Sender
    |
    | sends packets to 127.0.0.1:5000
    v
UDP Chaos Proxy
    |
    | forwards packets to 127.0.0.1:6000
    v
UDP Receiver
````

The proxy acts as a controlled unstable network layer between the data source and the final application.

## Configuration

The default configuration is defined directly in `proxy.py`:

```python
LISTEN_IP = "127.0.0.1"
LISTEN_PORT = 5000

TARGET_IP = "127.0.0.1"
TARGET_PORT = 6000
```

By default:

* The proxy listens on `127.0.0.1:5000`
* The receiver should listen on `127.0.0.1:6000`

## Network Modes

### Stable Mode

Stable mode simulates a normal connection.

Behavior:

```txt
Delay: 10 to 40 ms
Packet drop: false
```

Example log:

```txt
mode=STABLE forwarded=true delay_ms=23 size=148
```

### Degraded Mode

Degraded mode simulates an unstable connection with latency and packet loss.

Behavior:

```txt
Delay: 300 to 1200 ms
Packet drop: 25%
```

Example logs:

```txt
mode=DEGRADED forwarded=true delay_ms=814 size=148
mode=DEGRADED dropped=true
```

### Cut Mode

Cut mode simulates a complete signal loss.

Behavior:

```txt
Delay: 0 ms
Packet drop: 100%
```

Example log:

```txt
mode=CUT dropped=true
```

### Random Mode

Random mode automatically switches between stable, degraded, and cut behavior.

Probabilities:

```txt
70% STABLE
22% DEGRADED
8% CUT
```

This mode is useful for realistic demonstrations where the connection quality changes over time.

## Running the Proxy

```bash
python3 proxy.py
```

Expected output:

```txt
Network proxy running
Listening on 127.0.0.1:5000
Forwarding to 127.0.0.1:6000
Commands: n=stable, l=degraded, c=cut, r=random
```

## Keyboard Commands

While the proxy is running, type one of the following commands and press Enter:

| Command | Mode     |
| ------- | -------- |
| `n`     | Stable   |
| `l`     | Degraded |
| `c`     | Cut      |
| `r`     | Random   |
| `q`     | Quit     |

Example:

```txt
l
MODE = DEGRADED
```

## Quick Test

Open three terminals.

### Terminal 1: start the proxy

```bash
python3 proxy.py
```

### Terminal 2: listen for forwarded packets

```bash
nc -ul 6000
```

### Terminal 3: send a test packet

```bash
echo '{"test": true}' | nc -u 127.0.0.1 5000
```

If the proxy is in stable mode, the JSON packet should appear in the receiver terminal.

## Example Payload

A drone or simulation module can send JSON data like this:

```json
{
  "timestamp": 1710000000.123,
  "drone_id": "DRONE_01",
  "horizon_points": [
    { "x": 120, "y": 340 },
    { "x": 320, "y": 330 },
    { "x": 520, "y": 345 }
  ],
  "roll": 2.4,
  "pitch": -1.2,
  "yaw": 184.5
}
```

The sender sends this payload to:

```txt
127.0.0.1:5000
```

The final application listens on:

```txt
127.0.0.1:6000
```

The proxy sits between both components and simulates the network behavior.

## Usage in the Aegis Edge Project

In the Aegis Edge project, this proxy is used to simulate communication between a drone data source and the HUD application.

It helps demonstrate that the HUD can handle:

* Normal telemetry reception
* Delayed data
* Lost packets
* Signal cuts
* Connection instability
* Recovery after degraded communication

This makes the prototype more credible because the demo does not assume perfect network conditions.

## Suggested Improvements

Possible future improvements:

* Add command-line arguments for IP and port configuration
* Add a JSON configuration file
* Add live packet statistics
* Count received, forwarded, and dropped packets
* Add burst packet loss simulation
* Add stronger jitter simulation
* Add timestamped logs
* Add support for multiple UDP targets
* Add replay mode for recorded network conditions

