import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import 'package:video_player_media_kit/video_player_media_kit.dart';

import 'screens/hud_screen.dart';
import 'services/hud_controller.dart';
import 'theme/hud_theme.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  VideoPlayerMediaKit.ensureInitialized(linux: true);
  SystemChrome.setPreferredOrientations([
    DeviceOrientation.landscapeLeft,
    DeviceOrientation.landscapeRight,
    DeviceOrientation.portraitUp,
  ]);
  SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
  runApp(const AegisEdgeApp());
}

class AegisEdgeApp extends StatelessWidget {
  const AegisEdgeApp({super.key});

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (_) => HudController(),
      child: MaterialApp(
        title: 'Aegis Edge HUD',
        debugShowCheckedModeBanner: false,
        theme: AegisTheme.darkTheme,
        home: const HudScreen(),
      ),
    );
  }
}
