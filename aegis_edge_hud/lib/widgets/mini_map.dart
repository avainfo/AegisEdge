import 'dart:math';
import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import '../models/drone_state.dart';
import '../models/link_state.dart';
import '../theme/hud_theme.dart';

class MiniMap extends StatefulWidget {
  final DroneState state;
  static const int _trailMax = 60;

  const MiniMap({super.key, required this.state});

  @override
  State<MiniMap> createState() => _MiniMapState();
}

class _MiniMapState extends State<MiniMap> {
  final List<Offset> _trail = [];

  @override
  void didUpdateWidget(MiniMap old) {
    super.didUpdateWidget(old);
    if (widget.state.linkState != LinkState.lost) {
      _trail.add(Offset(widget.state.posX, widget.state.posY));
      if (_trail.length > MiniMap._trailMax) _trail.removeAt(0);
    }
  }

  @override
  Widget build(BuildContext context) {
    final accent = AegisColors.forState(widget.state.linkState);
    final isLost = widget.state.linkState == LinkState.lost;

    return Container(
      decoration: aegisPanel(borderColor: accent.withOpacity(0.3)),
      padding: const EdgeInsets.all(8),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(
            'TACTICAL MAP',
            style: GoogleFonts.exo2(
              color: accent.withOpacity(0.7),
              fontSize: 9,
              letterSpacing: 2.5,
              fontWeight: FontWeight.w600,
            ),
          ),
          const SizedBox(height: 6),
          SizedBox(
            width: 150,
            height: 150,
            child: CustomPaint(
              painter: _MapPainter(
                trail: _trail,
                currentPos: Offset(widget.state.posX, widget.state.posY),
                yaw: widget.state.yaw,
                linkState: widget.state.linkState,
                isLost: isLost,
              ),
            ),
          ),
          const SizedBox(height: 4),
          Text(
            isLost
                ? 'LAST KNOWN  X:${widget.state.posX.toStringAsFixed(1)}  Y:${widget.state.posY.toStringAsFixed(1)}'
                : 'X:${widget.state.posX.toStringAsFixed(1)}  Y:${widget.state.posY.toStringAsFixed(1)}',
            style: GoogleFonts.exo2(
              color: isLost
                  ? AegisColors.lostRed.withOpacity(0.7)
                  : AegisColors.textSecondary,
              fontSize: 9,
              letterSpacing: 1.2,
            ),
          ),
        ],
      ),
    );
  }
}

class _MapPainter extends CustomPainter {
  final List<Offset> trail;
  final Offset currentPos;
  final double yaw;
  final LinkState linkState;
  final bool isLost;


  static const _minX = 8.0, _maxX = 18.0, _minY = 5.0, _maxY = 14.0;

  const _MapPainter({
    required this.trail,
    required this.currentPos,
    required this.yaw,
    required this.linkState,
    required this.isLost,
  });

  Offset _toCanvas(Offset world, Size size) => Offset(
        (world.dx - _minX) / (_maxX - _minX) * size.width,
        (world.dy - _minY) / (_maxY - _minY) * size.height,
      );

  @override
  void paint(Canvas canvas, Size size) {
    final accent = AegisColors.forState(linkState);

    canvas.drawRect(
      Rect.fromLTWH(0, 0, size.width, size.height),
      Paint()..color = const Color(0xFF040C14),
    );

    final gp = Paint()
      ..color = AegisColors.gridLine
      ..strokeWidth = 0.5;
    for (int i = 0; i <= 5; i++) {
      final x = size.width * i / 5;
      final y = size.height * i / 5;
      canvas.drawLine(Offset(x, 0), Offset(x, size.height), gp);
      canvas.drawLine(Offset(0, y), Offset(size.width, y), gp);
    }

    if (trail.length > 1) {
      for (int i = 1; i < trail.length; i++) {
        final a = _toCanvas(trail[i - 1], size);
        final b = _toCanvas(trail[i], size);
        canvas.drawLine(
          a,
          b,
          Paint()
            ..color = accent.withOpacity(0.15 + 0.5 * i / trail.length)
            ..strokeWidth = 1.2,
        );
      }
    }

    final pos = _toCanvas(currentPos, size);

    if (isLost) {
      canvas.drawCircle(
        pos,
        14,
        Paint()
          ..color = AegisColors.lostRed.withOpacity(0.2)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 1.5,
      );
    }

    canvas.save();
    canvas.translate(pos.dx, pos.dy);
    canvas.rotate(yaw * pi / 180);
    final tri = Path()
      ..moveTo(0, -7)
      ..lineTo(5, 5)
      ..lineTo(0, 2)
      ..lineTo(-5, 5)
      ..close();
    canvas.drawPath(
        tri, Paint()..color = accent.withOpacity(isLost ? 0.4 : 0.9));
    canvas.restore();

    canvas.drawRect(
      Rect.fromLTWH(0, 0, size.width, size.height),
      Paint()
        ..color = AegisColors.panelBorder
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1,
    );
  }

  @override
  bool shouldRepaint(_MapPainter old) =>
      old.trail.length != trail.length ||
      old.currentPos != currentPos ||
      old.yaw != yaw ||
      old.linkState != linkState ||
      old.isLost != isLost;
}
