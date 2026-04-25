import 'dart:async';
import 'dart:convert';
import 'dart:io';

import '../models/drone_state.dart';


class UdpService {
  static const int port = 5005;

  RawDatagramSocket? _socket;
  final _controller = StreamController<DroneState>.broadcast();

  Stream<DroneState> get stream => _controller.stream;

  Future<bool> start() async {
    try {
      _socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, port);
      _socket!.listen((RawSocketEvent event) {
        if (event == RawSocketEvent.read) {
          final dg = _socket!.receive();
          if (dg == null) return;
          try {
            final json =
                jsonDecode(utf8.decode(dg.data)) as Map<String, dynamic>;
            _controller.add(DroneState.fromJson(json));
          } catch (_) {}
        }
      });
      return true;
    } catch (_) {
      return false; // Port unavailable — demo mode takes over
    }
  }

  void dispose() {
    _socket?.close();
    _controller.close();
  }
}
