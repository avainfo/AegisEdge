import 'package:flutter/material.dart';
import '../theme/hud_theme.dart';


class TacticalBgPainter extends CustomPainter {
  const TacticalBgPainter();

  @override
  void paint(Canvas canvas, Size size) {
    // Important: do not paint an opaque background here.
    // The drone video is rendered below this painter.

    final gridPaint = Paint()
      ..color = AegisColors.gridLine
      ..strokeWidth = 0.5;
    const spacing = 48.0;
    for (double x = 0; x < size.width; x += spacing) {
      canvas.drawLine(Offset(x, 0), Offset(x, size.height), gridPaint);
    }
    for (double y = 0; y < size.height; y += spacing) {
      canvas.drawLine(Offset(0, y), Offset(size.width, y), gridPaint);
    }

    final scanPaint = Paint()..color = const Color(0x06FFFFFF);
    for (double y = 0; y < size.height; y += 3) {
      canvas.drawLine(Offset(0, y), Offset(size.width, y), scanPaint);
    }

    final vignette = Paint()
      ..shader = RadialGradient(
        colors: [
          Colors.transparent,
          Colors.black.withOpacity(0.55),
        ],
        stops: const [0.5, 1.0],
      ).createShader(
          Rect.fromLTWH(0, 0, size.width, size.height));
    canvas.drawRect(
        Rect.fromLTWH(0, 0, size.width, size.height), vignette);
  }

  @override
  bool shouldRepaint(TacticalBgPainter _) => false;
}
