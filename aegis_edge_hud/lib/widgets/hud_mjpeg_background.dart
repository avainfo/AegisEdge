import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:provider/provider.dart';
import 'package:google_fonts/google_fonts.dart';

import '../services/hud_controller.dart';
import '../models/drone_state.dart';
import '../models/link_state.dart';
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
  bool _disposed = false;

  @override
  void initState() {
    super.initState();
    _scheduleNextFetch();
  }

  void _scheduleNextFetch() {
    if (!mounted || _disposed) return;
    
    final droneState = context.read<HudController>().state;
    _timer?.cancel();
    
    Duration interval;
    switch (droneState.videoState) {
      case "LIVE":
        interval = const Duration(milliseconds: 100);
        break;
      case "DEGRADED":
        interval = const Duration(milliseconds: 600);
        break;
      case "FROZEN":
        interval = _latestFrame == null 
            ? const Duration(milliseconds: 300) 
            : const Duration(milliseconds: 1000);
        break;
      case "UNAVAILABLE":
      default:
        interval = const Duration(milliseconds: 1000);
        break;
    }

    _timer = Timer(interval, _fetchFrame);
  }

  Future<void> _fetchFrame() async {
    if (_fetching || !mounted || _disposed) return;

    final droneState = context.read<HudController>().state;

    // Design Decision:
    // Image bytes are transported over HTTP through the Chaos Proxy to simulate 
    // real-world connection constraints (latency, packet loss, cut links).
    // The permission to fetch or freeze still comes from Aegis Core metadata.
    
    bool shouldAttemptFetch = droneState.videoShouldFetch;
    
    // Exception: If we have no frame yet, allow fetch even if frozen to get initial visual
    if (droneState.videoShouldFreeze && _latestFrame == null) {
      shouldAttemptFetch = true;
    }

    if (!shouldAttemptFetch) {
      _scheduleNextFetch();
      return;
    }

    _fetching = true;

    try {
      final response = await http
          .get(Uri.parse(droneState.frameEndpoint))
          .timeout(const Duration(milliseconds: 1000));

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
      _scheduleNextFetch();
    }
  }

  void _handleFailure() {
    _consecutiveFailures++;
    if (_consecutiveFailures > 15 && !_isFailing) {
      if (mounted) {
        setState(() {
          _isFailing = true;
        });
      }
    }
  }

  @override
  void dispose() {
    _disposed = true;
    _timer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final droneState = context.select<HudController, DroneState>((c) => c.state);
    final videoState = droneState.videoState;

    if (_isFailing) {
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

    Widget imageWidget = Image.memory(
      _latestFrame!,
      fit: BoxFit.cover,
      gaplessPlayback: true,
    );

    if (videoState == "DEGRADED") {
      imageWidget = Stack(
        fit: StackFit.expand,
        children: [
          imageWidget,
          Container(color: Colors.amber.withOpacity(0.15)),
          Positioned(
            top: 100,
            left: 40,
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
              color: Colors.black54,
              child: Text(
                'VIDEO DEGRADED',
                style: GoogleFonts.exo2(
                  color: Colors.amberAccent,
                  fontSize: 12,
                  fontWeight: FontWeight.bold,
                  letterSpacing: 1.5,
                ),
              ),
            ),
          ),
        ],
      );
    } else if (videoState == "FROZEN") {
      const ColorFilter greyscale = ColorFilter.matrix(<double>[
        0.2126, 0.7152, 0.0722, 0, 0,
        0.2126, 0.7152, 0.0722, 0, 0,
        0.2126, 0.7152, 0.0722, 0, 0,
        0,      0,      0,      1, 0,
      ]);

      imageWidget = Stack(
        fit: StackFit.expand,
        children: [
          ColorFiltered(
            colorFilter: greyscale,
            child: imageWidget,
          ),
          Container(color: Colors.black54),
          Center(
            child: Text(
              'VIDEO SIGNAL LOST',
              style: GoogleFonts.exo2(
                color: Colors.redAccent,
                fontSize: 32,
                fontWeight: FontWeight.bold,
                letterSpacing: 8,
              ),
            ),
          ),
        ],
      );
    } else if (videoState == "UNAVAILABLE") {
       imageWidget = Stack(
        fit: StackFit.expand,
        children: [
          Container(color: Colors.black),
          Center(
            child: Text(
              'VIDEO UNAVAILABLE',
              style: GoogleFonts.exo2(
                color: Colors.white54,
                fontSize: 24,
                fontWeight: FontWeight.bold,
                letterSpacing: 4,
              ),
            ),
          ),
        ],
      );
    }

    return SizedBox.expand(
      child: imageWidget,
    );
  }
}
