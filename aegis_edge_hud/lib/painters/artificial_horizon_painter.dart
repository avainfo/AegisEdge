import 'dart:math';
import 'package:flutter/material.dart';
import '../theme/hud_theme.dart';


class ArtificialHorizonPainter extends CustomPainter {
  final double roll;   // degrees
  final double pitch;  // degrees

  const ArtificialHorizonPainter({required this.roll, required this.pitch});

  @override
  void paint(Canvas canvas, Size size) {
    final cx = size.width / 2;
    final cy = size.height / 2;
    final r = min(cx, cy) - 4;

    canvas.save();
    canvas.translate(cx, cy);

    canvas.clipPath(Path()..addOval(Rect.fromCircle(center: Offset.zero, radius: r)));

    canvas.rotate(-roll * pi / 180);

    final pitchOffset = pitch * r / 30;

    canvas.drawRect(
      Rect.fromLTWH(-r * 2, -r * 2 + pitchOffset, r * 4, r * 2),
      Paint()..color = AegisColors.skyColor,
    );
    canvas.drawRect(
      Rect.fromLTWH(-r * 2, pitchOffset, r * 4, r * 2),
      Paint()..color = AegisColors.groundColor,
    );

    canvas.drawLine(
      Offset(-r, pitchOffset),
      Offset(r, pitchOffset),
      Paint()
        ..color = AegisColors.normalGreen
        ..strokeWidth = 1.5,
    );

    final tickPaint = Paint()
      ..color = AegisColors.textSecondary.withOpacity(0.5)
      ..strokeWidth = 1;
    for (int deg = -20; deg <= 20; deg += 5) {
      if (deg == 0) continue;
      final y = pitchOffset + deg * r / 30;
      final tickW = deg % 10 == 0 ? r * 0.35 : r * 0.2;
      canvas.drawLine(Offset(-tickW, y), Offset(tickW, y), tickPaint);
    }

    canvas.restore();

    canvas.drawCircle(
      Offset(cx, cy),
      r,
      Paint()
        ..color = AegisColors.panelBorder
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.5,
    );

    final crossPaint = Paint()
      ..color = AegisColors.normalGreen
      ..strokeWidth = 2;
    final cw = r * 0.25;
    canvas.drawLine(Offset(cx - r * 0.55, cy), Offset(cx - cw, cy), crossPaint);
    canvas.drawLine(Offset(cx + cw, cy), Offset(cx + r * 0.55, cy), crossPaint);
    canvas.drawCircle(Offset(cx, cy), 3, Paint()..color = AegisColors.normalGreen);

    canvas.save();
    canvas.translate(cx, cy);
    final arcPaint = Paint()
      ..color = AegisColors.normalGreen.withOpacity(0.5)
      ..strokeWidth = 2
      ..style = PaintingStyle.stroke;
    canvas.drawArc(
      Rect.fromCircle(center: Offset.zero, radius: r - 4),
      -pi / 2 - 30 * pi / 180,
      60 * pi / 180,
      false,
      arcPaint,
    );
    canvas.rotate(-roll * pi / 180);
    canvas.drawLine(
      Offset(0, -(r - 10)),
      Offset(0, -(r - 2)),
      Paint()
        ..color = AegisColors.normalGreen
        ..strokeWidth = 2,
    );
    canvas.restore();
  }

  @override
  bool shouldRepaint(ArtificialHorizonPainter old) =>
      old.roll != roll || old.pitch != pitch;
}
