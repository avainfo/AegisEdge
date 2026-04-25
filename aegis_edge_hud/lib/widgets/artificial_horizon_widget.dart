import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import '../models/drone_state.dart';
import '../painters/artificial_horizon_painter.dart';
import '../theme/hud_theme.dart';

class ArtificialHorizonWidget extends StatelessWidget {
  final DroneState state;

  const ArtificialHorizonWidget({super.key, required this.state});

  @override
  Widget build(BuildContext context) {
    final accent = AegisColors.forState(state.linkState);

    return Container(
      decoration: aegisPanel(borderColor: accent.withOpacity(0.3)),
      padding: const EdgeInsets.all(10),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(
            'ATTITUDE',
            style: GoogleFonts.exo2(
              color: accent.withOpacity(0.7),
              fontSize: 9,
              letterSpacing: 2.5,
              fontWeight: FontWeight.w600,
            ),
          ),
          const SizedBox(height: 6),
          SizedBox(
            width: 140,
            height: 140,
            child: CustomPaint(
              painter: ArtificialHorizonPainter(
                roll: state.roll,
                pitch: state.pitch,
              ),
            ),
          ),
          const SizedBox(height: 6),
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceAround,
            children: [
              _miniVal('R', '${state.roll.toStringAsFixed(1)}°'),
              _miniVal('P', '${state.pitch.toStringAsFixed(1)}°'),
              _miniVal('Y', '${state.yaw.toStringAsFixed(0)}°'),
            ],
          ),
        ],
      ),
    );
  }

  Widget _miniVal(String label, String value) => Column(
        children: [
          Text(label,
              style: GoogleFonts.exo2(
                  color: AegisColors.textDim,
                  fontSize: 9,
                  letterSpacing: 1.5)),
          Text(value,
              style: GoogleFonts.exo2(
                  color: AegisColors.textSecondary,
                  fontSize: 11,
                  fontWeight: FontWeight.w600)),
        ],
      );
}
