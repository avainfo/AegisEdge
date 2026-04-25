import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import '../models/drone_state.dart';
import '../models/link_state.dart';
import '../theme/hud_theme.dart';

class TelemetryPanel extends StatelessWidget {
  final DroneState state;

  const TelemetryPanel({super.key, required this.state});

  @override
  Widget build(BuildContext context) {
    final accent = AegisColors.forState(state.linkState);
    final isLost = state.linkState == LinkState.lost;

    return Container(
      decoration: aegisPanel(borderColor: accent.withOpacity(0.3)),
      padding: const EdgeInsets.all(12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: [
          _sectionLabel('TELEMETRY', accent),
          const SizedBox(height: 8),
          _row('ALT', '${state.altitude.toStringAsFixed(1)}', 'm', accent, isLost),
          _row('ROLL', '${state.roll.toStringAsFixed(1)}', '°', accent, isLost),
          _row('PITCH', '${state.pitch.toStringAsFixed(1)}', '°', accent, isLost),
          _row('YAW', '${state.yaw.toStringAsFixed(1)}', '°', accent, isLost),
          const SizedBox(height: 4),
          Divider(color: AegisColors.panelBorder, height: 12),
          _sectionLabel('LINK', accent),
          const SizedBox(height: 4),
          _latencyRow(state.latencyMs, state.linkState),
          if (state.stale) ...[
            const SizedBox(height: 6),
            _staleTag(),
          ],
        ],
      ),
    );
  }

  Widget _sectionLabel(String label, Color accent) => Text(
        label,
        style: GoogleFonts.exo2(
          color: accent.withOpacity(0.7),
          fontSize: 9,
          letterSpacing: 2.5,
          fontWeight: FontWeight.w600,
        ),
      );

  Widget _row(String label, String value, String unit, Color accent, bool stale) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 3),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(
            label,
            style: GoogleFonts.exo2(
              color: AegisColors.textSecondary,
              fontSize: 11,
              letterSpacing: 1.5,
            ),
          ),
          Row(
            children: [
              Text(
                value,
                style: GoogleFonts.exo2(
                  color: stale
                      ? AegisColors.textSecondary.withOpacity(0.5)
                      : AegisColors.textPrimary,
                  fontSize: 14,
                  fontWeight: FontWeight.w600,
                  decoration: stale ? TextDecoration.lineThrough : null,
                  decorationColor: AegisColors.lostRed.withOpacity(0.5),
                ),
              ),
              const SizedBox(width: 2),
              Text(
                unit,
                style: GoogleFonts.exo2(
                  color: AegisColors.textDim,
                  fontSize: 10,
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _latencyRow(int ms, LinkState state) {
    final color = AegisColors.forState(state);
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        Text('LATENCY',
            style: GoogleFonts.exo2(
                color: AegisColors.textSecondary,
                fontSize: 11,
                letterSpacing: 1.5)),
        Text(
          state == LinkState.lost ? '---' : '${ms}ms',
          style: GoogleFonts.exo2(
              color: color, fontSize: 13, fontWeight: FontWeight.w700),
        ),
      ],
    );
  }

  Widget _staleTag() => Container(
        padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 3),
        decoration: BoxDecoration(
          color: AegisColors.lostRed.withOpacity(0.12),
          borderRadius: BorderRadius.circular(3),
          border: Border.all(color: AegisColors.lostRed.withOpacity(0.3)),
        ),
        child: Text(
          '⚠ STALE DATA',
          style: GoogleFonts.exo2(
            color: AegisColors.lostRed,
            fontSize: 9,
            letterSpacing: 1.8,
            fontWeight: FontWeight.w600,
          ),
        ),
      );
}
