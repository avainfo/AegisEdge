import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import '../models/link_state.dart';

class AegisColors {
  AegisColors._();

  static const Color background = Color(0xFF050A0F);
  static const Color backgroundSecondary = Color(0xFF0A1520);
  static const Color panelBg = Color(0xCC061018);

  static const Color normalGreen = Color(0xFF00F5A0);
  static const Color normalGreenDim = Color(0x2200F5A0);
  static const Color degradedAmber = Color(0xFFFFB800);
  static const Color degradedAmberDim = Color(0x22FFB800);
  static const Color lostRed = Color(0xFFFF3B30);
  static const Color lostRedDim = Color(0x22FF3B30);

  static const Color textPrimary = Color(0xFFE8F4F8);
  static const Color textSecondary = Color(0xFF7A9BB5);
  static const Color textDim = Color(0xFF2A4560);

  static const Color gridLine = Color(0x0F00E87A);
  static const Color panelBorder = Color(0x3300F5A0);
  static const Color skyColor = Color(0xFF061828);
  static const Color groundColor = Color(0xFF1A0A00);

  static Color forState(LinkState state) => switch (state) {
        LinkState.normal => normalGreen,
        LinkState.degraded => degradedAmber,
        LinkState.lost => lostRed,
      };

  static Color dimForState(LinkState state) => switch (state) {
        LinkState.normal => normalGreenDim,
        LinkState.degraded => degradedAmberDim,
        LinkState.lost => lostRedDim,
      };
}

class AegisTheme {
  AegisTheme._();

  static ThemeData get darkTheme {
    final base = ThemeData.dark(useMaterial3: true);
    return base.copyWith(
      scaffoldBackgroundColor: AegisColors.background,
      textTheme: GoogleFonts.exo2TextTheme(base.textTheme).apply(
        bodyColor: AegisColors.textPrimary,
        displayColor: AegisColors.textPrimary,
      ),
      colorScheme: const ColorScheme.dark(
        primary: AegisColors.normalGreen,
        secondary: AegisColors.degradedAmber,
        error: AegisColors.lostRed,
        surface: AegisColors.backgroundSecondary,
        onSurface: AegisColors.textPrimary,
      ),
    );
  }
}

BoxDecoration aegisPanel({Color? borderColor}) => BoxDecoration(
      color: AegisColors.panelBg,
      borderRadius: BorderRadius.circular(8),
      border: Border.all(
        color: borderColor ?? AegisColors.panelBorder,
        width: 1,
      ),
    );

class CornerBracketsPainter extends CustomPainter {
  final Color color;
  final double size;
  final double strokeWidth;

  const CornerBracketsPainter({
    required this.color,
    this.size = 24,
    this.strokeWidth = 2,
  });

  @override
  void paint(Canvas canvas, Size canvasSize) {
    final paint = Paint()
      ..color = color
      ..strokeWidth = strokeWidth
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.square;

    final w = canvasSize.width;
    final h = canvasSize.height;
    final s = size;

    canvas.drawLine(Offset(0, s), Offset(0, 0), paint);
    canvas.drawLine(Offset(0, 0), Offset(s, 0), paint);
    canvas.drawLine(Offset(w - s, 0), Offset(w, 0), paint);
    canvas.drawLine(Offset(w, 0), Offset(w, s), paint);
    canvas.drawLine(Offset(0, h - s), Offset(0, h), paint);
    canvas.drawLine(Offset(0, h), Offset(s, h), paint);
    canvas.drawLine(Offset(w - s, h), Offset(w, h), paint);
    canvas.drawLine(Offset(w, h), Offset(w, h - s), paint);
  }

  @override
  bool shouldRepaint(CornerBracketsPainter old) =>
      old.color != color || old.size != size || old.strokeWidth != strokeWidth;
}
