import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import '../models/link_state.dart';
import '../theme/hud_theme.dart';


class StaleOverlay extends StatefulWidget {
  final LinkState linkState;
  final bool stale;
  final int lastUpdateMs;

  const StaleOverlay({
    super.key,
    required this.linkState,
    required this.stale,
    required this.lastUpdateMs,
  });

  @override
  State<StaleOverlay> createState() => _StaleOverlayState();
}

class _StaleOverlayState extends State<StaleOverlay>
    with SingleTickerProviderStateMixin {
  late AnimationController _ctrl;
  late Animation<double> _fade;

  @override
  void initState() {
    super.initState();
    _ctrl = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 700),
    )..repeat(reverse: true);
    _fade = Tween<double>(begin: 0.3, end: 0.9).animate(
      CurvedAnimation(parent: _ctrl, curve: Curves.easeInOut),
    );
  }

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final show = widget.stale || widget.linkState == LinkState.lost;
    if (!show) return const SizedBox.shrink();

    final isLost = widget.linkState == LinkState.lost;
    final borderColor = isLost ? AegisColors.lostRed : AegisColors.degradedAmber;
    final label = isLost ? 'SIGNAL LOST' : 'STALE DATA';

    return AnimatedBuilder(
      animation: _fade,
      builder: (_, __) => IgnorePointer(
        child: Stack(
          children: [
            Container(
              decoration: BoxDecoration(
                border: Border.all(
                  color: borderColor.withOpacity(_fade.value * 0.6),
                  width: 3,
                ),
              ),
            ),
            Positioned(
              top: 56,
              left: 0,
              right: 0,
              child: Center(
                child: Container(
                  padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 5),
                  decoration: BoxDecoration(
                    color: borderColor.withOpacity(0.15),
                    border: Border.all(
                        color: borderColor.withOpacity(_fade.value * 0.7)),
                    borderRadius: BorderRadius.circular(4),
                  ),
                  child: Text(
                    '⚠  $label  —  LAST UPDATE ${(widget.lastUpdateMs / 1000).toStringAsFixed(1)}s',
                    style: GoogleFonts.exo2(
                      color: borderColor.withOpacity(_fade.value),
                      fontSize: 11,
                      letterSpacing: 2.0,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
