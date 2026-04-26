import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import '../models/link_state.dart';
import '../theme/hud_theme.dart';


class StatusBadge extends StatefulWidget {
  final LinkState linkState;
  final bool demoMode;

  const StatusBadge({super.key, required this.linkState, this.demoMode = false});

  @override
  State<StatusBadge> createState() => _StatusBadgeState();
}

class _StatusBadgeState extends State<StatusBadge>
    with SingleTickerProviderStateMixin {
  late AnimationController _pulseCtrl;
  late Animation<double> _pulse;

  @override
  void initState() {
    super.initState();
    _pulseCtrl = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 900),
    )..repeat(reverse: true);
    _pulse = Tween<double>(begin: 0.3, end: 1.0).animate(
      CurvedAnimation(parent: _pulseCtrl, curve: Curves.easeInOut),
    );
  }

  @override
  void dispose() {
    _pulseCtrl.dispose();
    super.dispose();
  }

  @override
  void didUpdateWidget(StatusBadge old) {
    super.didUpdateWidget(old);
    if (old.linkState != widget.linkState) {
      final speed = switch (widget.linkState) {
        LinkState.normal => const Duration(milliseconds: 2000),
        LinkState.degraded => const Duration(milliseconds: 800),
        LinkState.lost => const Duration(milliseconds: 350),
      };
      _pulseCtrl.duration = speed;
      _pulseCtrl.repeat(reverse: true);
    }
  }

  @override
  Widget build(BuildContext context) {
    final color = AegisColors.forState(widget.linkState);
    final label = widget.linkState.displayLabel;

    return AnimatedSwitcher(
      duration: const Duration(milliseconds: 400),
      child: Container(
        key: ValueKey(widget.linkState),
        padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 8),
        decoration: BoxDecoration(
          color: color.withOpacity(0.08),
          borderRadius: BorderRadius.circular(4),
          border: Border.all(color: color.withOpacity(0.4), width: 1),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            AnimatedBuilder(
              animation: _pulse,
              builder: (_, __) => Container(
                width: 8,
                height: 8,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  color: color.withOpacity(_pulse.value),
                  boxShadow: [
                    BoxShadow(
                      color: color.withOpacity(_pulse.value * 0.6),
                      blurRadius: 6,
                      spreadRadius: 2,
                    ),
                  ],
                ),
              ),
            ),
            const SizedBox(width: 10),
            Text(
              label,
              style: GoogleFonts.exo2(
                color: color,
                fontSize: 13,
                fontWeight: FontWeight.w600,
                letterSpacing: 2.0,
              ),
            ),
            if (widget.demoMode) ...[
              const SizedBox(width: 12),
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                decoration: BoxDecoration(
                  color: AegisColors.textDim.withOpacity(0.3),
                  borderRadius: BorderRadius.circular(3),
                ),
                child: Text(
                  'DEMO',
                  style: GoogleFonts.exo2(
                    color: AegisColors.textSecondary,
                    fontSize: 9,
                    letterSpacing: 1.5,
                  ),
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}
