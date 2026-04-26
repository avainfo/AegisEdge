import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';

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
          // Drain all available datagrams, keep only the latest
          DroneState? latest;
          while (true) {
            final dg = _socket!.receive();
            if (dg == null) break;
            try {
              final payload = utf8.decode(dg.data);
              final json = jsonDecode(payload) as Map<String, dynamic>;
              latest = DroneState.fromJson(json);
            } catch (e) {
              debugPrint('UDP parse error: $e');
            }
          }
          if (latest != null) _controller.add(latest);
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
