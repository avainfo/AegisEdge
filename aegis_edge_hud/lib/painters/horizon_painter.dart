import 'package:flutter/material.dart';

import '../models/horizon_data.dart';
import '../models/link_state.dart';
import '../theme/hud_theme.dart';


class HorizonPainter extends CustomPainter {
  final HorizonData horizon;
  final LinkState linkState;

  const HorizonPainter({required this.horizon, required this.linkState});

  @override
  void paint(Canvas canvas, Size size) {
    if (!horizon.detected || horizon.points.length < 2) return;

    final color = AegisColors.forState(linkState);
    final opacity = linkState == LinkState.lost ? 0.30 : 1.0;

    final pts =
        horizon.points.map((p) => Offset(p.x * size.width, p.y * size.height)).toList();

    final path = Path()..moveTo(pts.first.dx, pts.first.dy);
    for (int i = 1; i < pts.length; i++) {
      path.lineTo(pts[i].dx, pts[i].dy);
    }

    // Simulated glow via layered strokes — avoids expensive GPU blur pass
    canvas.drawPath(
      path,
      Paint()
        ..color = color.withOpacity(0.08 * opacity)
        ..strokeWidth = 16
        ..style = PaintingStyle.stroke
        ..strokeCap = StrokeCap.round,
    );
    canvas.drawPath(
      path,
      Paint()
        ..color = color.withOpacity(0.18 * opacity)
        ..strokeWidth = 8
        ..style = PaintingStyle.stroke
        ..strokeCap = StrokeCap.round,
    );

    if (linkState == LinkState.degraded) {
      _drawDashed(canvas, pts, color.withOpacity(opacity), 2.0, 12, 6);
    } else {
      canvas.drawPath(
        path,
        Paint()
          ..color = color.withOpacity(opacity)
          ..strokeWidth = 2.0
          ..style = PaintingStyle.stroke
          ..strokeCap = StrokeCap.round,
      );
    }

    for (final p in pts) {
      canvas.drawCircle(
        p,
        3.5,
        Paint()..color = color.withOpacity(opacity),
      );
    }

    if (linkState != LinkState.lost) {
      final pct = (horizon.confidence * 100).toStringAsFixed(0);
      final tp = TextPainter(
        text: TextSpan(
          text: 'CONF $pct%',
          style: TextStyle(
            color: color.withOpacity(0.8),
            fontSize: 11,
            fontFamily: 'Exo2',
            letterSpacing: 1.4,
          ),
        ),
        textDirection: TextDirection.ltr,
      )..layout();
      tp.paint(canvas, Offset(pts.last.dx + 8, pts.last.dy - 16));
    }
  }

  void _drawDashed(Canvas canvas, List<Offset> pts, Color color,
      double strokeW, double dashLen, double gapLen) {
    final paint = Paint()
      ..color = color
      ..strokeWidth = strokeW
      ..strokeCap = StrokeCap.round;

    for (int i = 0; i < pts.length - 1; i++) {
      final p1 = pts[i];
      final p2 = pts[i + 1];
      final dx = p2.dx - p1.dx;
      final dy = p2.dy - p1.dy;
      final len = (p2 - p1).distance;
      final unitX = dx / len;
      final unitY = dy / len;

      double drawn = 0;
      bool drawing = true;
      while (drawn < len) {
        final segLen = drawing
            ? (dashLen).clamp(0, len - drawn)
            : (gapLen).clamp(0, len - drawn);
        if (drawing) {
          final start = Offset(p1.dx + unitX * drawn, p1.dy + unitY * drawn);
          final end = Offset(
              p1.dx + unitX * (drawn + segLen), p1.dy + unitY * (drawn + segLen));
          canvas.drawLine(start, end, paint);
        }
        drawn += segLen;
        drawing = !drawing;
      }
    }
  }

  @override
  bool shouldRepaint(HorizonPainter old) =>
      old.horizon != horizon || old.linkState != linkState;
}
