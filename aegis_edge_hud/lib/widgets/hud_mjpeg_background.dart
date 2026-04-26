import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'hud_video_background.dart';

/// Background that fetches live frames from AirSim Python Bridge.
/// 
/// Limitation Documented:
/// `flutter_mjpeg` was incompatible with the current `google_fonts` HTTP dependency.
/// To keep dependencies clean and minimal as requested, this widget polls
/// the `/snapshot` endpoint every 100ms (10 FPS) instead of parsing the raw MJPEG stream.
/// This achieves the same visual result robustly.
class HudMjpegBackground extends StatefulWidget {
  const HudMjpegBackground({super.key});

  @override
  State<HudMjpegBackground> createState() => _HudMjpegBackgroundState();
}

class _HudMjpegBackgroundState extends State<HudMjpegBackground> {
  Timer? _timer;
  Uint8List? _latestFrame;
  bool _isFailing = false;
  int _consecutiveFailures = 0;
  bool _fetching = false;

  @override
  void initState() {
    super.initState();
    _startPolling();
  }

  void _startPolling() {
    _timer = Timer.periodic(const Duration(milliseconds: 100), (_) {
      _fetchFrame();
    });
  }

  Future<void> _fetchFrame() async {
    if (_fetching) return;
    _fetching = true;

    try {
      final response = await http
          .get(Uri.parse('http://127.0.0.1:8080/snapshot'))
          .timeout(const Duration(milliseconds: 200));

      if (response.statusCode == 200) {
        if (mounted) {
          setState(() {
            _latestFrame = response.bodyBytes;
            _isFailing = false;
            _consecutiveFailures = 0;
          });
        }
      } else {
        _handleFailure();
      }
    } catch (e) {
      _handleFailure();
    } finally {
      _fetching = false;
    }
  }

  void _handleFailure() {
    _consecutiveFailures++;
    if (_consecutiveFailures > 10 && !_isFailing) {
      if (mounted) {
        setState(() {
          _isFailing = true;
        });
      }
    }
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (_isFailing) {
      // Fallback gracefully to the local MP4 background
      return const HudVideoBackground();
    }

    if (_latestFrame == null) {
      return const ColoredBox(
        color: Colors.black,
        child: Center(
          child: Text(
            'CONNECTING TO AIRSIM...',
            style: TextStyle(
              color: Colors.white70,
              fontSize: 14,
              letterSpacing: 2,
            ),
          ),
        ),
      );
    }

    return SizedBox.expand(
      child: Image.memory(
        _latestFrame!,
        fit: BoxFit.cover,
        gaplessPlayback: true,
      ),
    );
  }
}
