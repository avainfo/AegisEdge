import 'dart:async';
import 'dart:math';

import '../models/drone_state.dart';
import '../models/horizon_data.dart';
import '../models/link_state.dart';


class DemoService {
  static const _cycleSec = 22.0;
  static const _tickMs = 80;

  final _controller = StreamController<DroneState>.broadcast();
  Timer? _timer;
  int _elapsedMs = 0;
  int _frameIdCounter = 0;
  DroneState? _lastGoodState;

  Stream<DroneState> get stream => _controller.stream;

  void start() {
    _elapsedMs = 0;
    _timer = Timer.periodic(const Duration(milliseconds: _tickMs), _tick);
  }

  void stop() {
    _timer?.cancel();
    _timer = null;
  }

  void dispose() {
    stop();
    _controller.close();
  }

  void _tick(Timer _) {
    _elapsedMs += _tickMs;
    final t = (_elapsedMs / 1000.0) % _cycleSec;

    DroneState state;
    if (t < 7) {
      state = _generate(t, LinkState.normal, (40 + sin(t) * 15).toInt(), false);
      _lastGoodState = state;
    } else if (t < 14) {
      final dt = t - 7;
      final lat = (350 + dt * 220).toInt(); // 350 → 1890 ms
      state = _generate(t, LinkState.degraded, lat, dt > 3);
      _lastGoodState = state;
    } else if (t < 19) {
      state = _makeLost(_lastGoodState);
    } else {
      state = _generate(t, LinkState.normal, (55 + sin(t) * 8).toInt(), false);
      _lastGoodState = state;
    }

    if (!_controller.isClosed) _controller.add(state);
  }

  DroneState _generate(
      double t, LinkState linkState, int latencyMs, bool stale) {
    final roll = sin(t * 0.7) * 5.2;
    final pitch = cos(t * 0.45) * 3.1;
    final yaw = (t * 10.0) % 360;
    final altitude = 42.5 + sin(t * 0.25) * 2.4;
    final posX = 12.4 + sin(t * 0.18) * 1.8;
    final posY = 8.7 + cos(t * 0.13) * 1.4;

    final hy = 0.42 + pitch * 0.018;
    final tilt = roll * 0.004;

    return DroneState(
      frameId: ++_frameIdCounter,
      timestampMs: DateTime.now().millisecondsSinceEpoch,
      linkState: linkState,
      latencyMs: latencyMs,
      stale: stale,
      sourceValid: true,
      lastUpdateMs: _elapsedMs,
      posX: posX,
      posY: posY,
      roll: roll,
      pitch: pitch,
      yaw: yaw,
      altitude: altitude,
      horizon: HorizonData(
        detected: true,
        confidence: stale ? 0.52 : 0.78,
        points: [
          HorizonPoint(x: 0.05, y: hy + tilt * (-0.45)),
          HorizonPoint(x: 0.30, y: hy + tilt * (-0.20)),
          HorizonPoint(x: 0.55, y: hy),
          HorizonPoint(x: 0.80, y: hy + tilt * 0.25),
          HorizonPoint(x: 0.95, y: hy + tilt * 0.45),
        ],
      ),
    );
  }

  DroneState _makeLost(DroneState? last) {
    if (last == null) return DroneState.initial();
    return DroneState(
      frameId: last.frameId,
      timestampMs: last.timestampMs,
      linkState: LinkState.lost,
      latencyMs: last.latencyMs,
      stale: true,
      sourceValid: false,
      lastUpdateMs: last.lastUpdateMs,
      posX: last.posX,
      posY: last.posY,
      roll: last.roll,
      pitch: last.pitch,
      yaw: last.yaw,
      altitude: last.altitude,
      horizon: last.horizon,
    );
  }
}
