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

  @override
  void initState() {
    super.initState();
    _initVideo();
  }

  Future<void> _initVideo() async {
    try {
      final ctrl = VideoPlayerController.asset('assets/videos/drone_demo.mp4');
      await ctrl.initialize();
      ctrl.setLooping(true);
      ctrl.setVolume(0.0);
      ctrl.play();

      if (mounted) {
        setState(() {
          _controller = ctrl;
          _initialized = true;
        });
      } else {
        ctrl.dispose();
      }
    } catch (_) {
      // Video asset missing or unreadable — degrade gracefully to black bg.
    }
  }

  @override
  void dispose() {
    _controller?.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (!_initialized || _controller == null) {
      return const ColoredBox(color: Colors.black);
    }

    return SizedBox.expand(
      child: FittedBox(
        fit: BoxFit.cover,
        child: SizedBox(
          width: _controller!.value.size.width,
          height: _controller!.value.size.height,
          child: VideoPlayer(_controller!),
        ),
      ),
    );
  }
}
