import 'dart:async';

import 'package:flutter/foundation.dart';

import '../models/drone_state.dart';
import 'demo_service.dart';
import 'udp_service.dart';


class HudController extends ChangeNotifier {
  static const _udpTimeout = Duration(seconds: 3);

  final _udp = UdpService();
  final _demo = DemoService();

  DroneState _state = DroneState.initial();
  bool _demoActive = false;
  bool _initialized = false;

  StreamSubscription<DroneState>? _udpSub;
  StreamSubscription<DroneState>? _demoSub;

  DroneState get state => _state;
  bool get demoActive => _demoActive;

  Future<void> init() async {
    if (_initialized) return;
    _initialized = true;

    final udpOk = await _udp.start();

    if (udpOk) {
      _udpSub = _udp.stream.listen(_onUdpPacket);
    }
  }

  void _onUdpPacket(DroneState s) {
    if (_demoActive) _deactivateDemo();
    _setState(s);
  }

  void _activateDemo() {
    if (_demoActive) return;
    _demoActive = true;
    _demo.start();
    _demoSub = _demo.stream.listen(_setState);
    notifyListeners();
  }

  void _deactivateDemo() {
    _demoActive = false;
    _demo.stop();
    _demoSub?.cancel();
    _demoSub = null;
    notifyListeners();
  }

  void _setState(DroneState s) {
    _state = s;
    notifyListeners();
  }

  @override
  void dispose() {
    _udpSub?.cancel();
    _demoSub?.cancel();
    _udp.dispose();
    _demo.dispose();
    super.dispose();
  }
}
