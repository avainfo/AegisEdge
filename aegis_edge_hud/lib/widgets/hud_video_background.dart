import 'package:flutter/material.dart';
import 'package:video_player/video_player.dart';

/// Full-screen video background for the HUD demo.
///
/// Isolated in its own StatefulWidget so that telemetry updates never
/// trigger a rebuild here — the controller lives entirely in this subtree.
class HudVideoBackground extends StatefulWidget {
  const HudVideoBackground({super.key});

  @override
  State<HudVideoBackground> createState() => _HudVideoBackgroundState();
}

class _HudVideoBackgroundState extends State<HudVideoBackground> {
  VideoPlayerController? _controller;
  bool _initialized = false;
  String? _error;

  @override
  void initState() {
    super.initState();
    _initVideo();
  }

  Future<void> _initVideo() async {
    try {
      final ctrl = VideoPlayerController.asset('assets/videos/drone_demo.mp4');
      await ctrl.initialize();
      await ctrl.setLooping(true);
      await ctrl.setVolume(0.0);
      await ctrl.play();

      if (mounted) {
        setState(() {
          _controller = ctrl;
          _initialized = true;
        });
      } else {
        ctrl.dispose();
      }
    } catch (e) {
      debugPrint('Video init error: $e');
      if (mounted) {
        setState(() {
          _error = e.toString();
        });
      }
    }
  }

  @override
  void dispose() {
    _controller?.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (_error != null) {
      return ColoredBox(
        color: Colors.black,
        child: Center(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const Icon(Icons.error_outline, color: Colors.red, size: 48),
              const SizedBox(height: 16),
              const Text(
                'VIDEO LOAD FAILED',
                style: TextStyle(color: Colors.white, fontSize: 18, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 8),
              const Text(
                'Check assets/videos/drone_demo.mp4',
                style: TextStyle(color: Colors.white70, fontSize: 14),
              ),
              const SizedBox(height: 8),
              Text(
                _error!,
                style: const TextStyle(color: Colors.white54, fontSize: 10),
                textAlign: TextAlign.center,
              ),
            ],
          ),
        ),
      );
    }

    if (!_initialized || _controller == null) {
      return const ColoredBox(
        color: Colors.black,
        child: Center(
          child: Text(
            'LOADING VIDEO...',
            style: TextStyle(color: Colors.white70, fontSize: 14, letterSpacing: 2),
          ),
        ),
      );
    }

    final width = _controller!.value.size.width;
    final height = _controller!.value.size.height;

    if (width <= 0 || height <= 0) {
      return ColoredBox(
        color: Colors.black,
        child: Center(
          child: Text(
            'VIDEO LOAD FAILED\nInvalid Dimensions: ${width}x$height',
            style: const TextStyle(color: Colors.red, fontSize: 14),
            textAlign: TextAlign.center,
          ),
        ),
      );
    }

    return SizedBox.expand(
      child: FittedBox(
        fit: BoxFit.cover,
        child: SizedBox(
          width: width,
          height: height,
          child: VideoPlayer(_controller!),
        ),
      ),
    );
  }
}
