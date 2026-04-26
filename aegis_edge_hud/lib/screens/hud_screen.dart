import 'dart:async';

import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:provider/provider.dart';

import '../models/drone_state.dart';
import '../models/link_state.dart';
import '../painters/horizon_painter.dart';
import '../painters/tactical_bg_painter.dart';
import '../services/hud_controller.dart';
import '../theme/hud_theme.dart';
import '../widgets/artificial_horizon_widget.dart';
import '../widgets/mini_map.dart';
import '../widgets/stale_overlay.dart';
import '../widgets/status_badge.dart';
import '../widgets/telemetry_panel.dart';

class HudScreen extends StatefulWidget {
  const HudScreen({super.key});

  @override
  State<HudScreen> createState() => _HudScreenState();
}

class _HudScreenState extends State<HudScreen> {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      context.read<HudController>().init();
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AegisColors.background,
      body: LayoutBuilder(
        builder: (context, constraints) {
          final isWide = constraints.maxWidth > 700;
          return Stack(
            children: [
              // Static — never rebuilds, isolated in its own layer
              const Positioned.fill(
                child: RepaintBoundary(
                  child: CustomPaint(painter: TacticalBgPainter()),
                ),
              ),

              // Dynamic — only rebuilt when HudController notifies
              Selector<HudController, (DroneState, bool)>(
                selector: (_, ctrl) => (ctrl.state, ctrl.demoActive),
                builder: (context, data, _) {
                  final s = data.$1;
                  final demoActive = data.$2;
                  final accent = AegisColors.forState(s.linkState);
                  return Stack(
                    children: [
                      Positioned.fill(
                        child: CustomPaint(
                          painter: HorizonPainter(
                            horizon: s.horizon,
                            linkState: s.linkState,
                          ),
                        ),
                      ),

                      Positioned.fill(
                        child: StaleOverlay(
                          linkState: s.linkState,
                          stale: s.stale,
                          lastUpdateMs: s.lastUpdateMs,
                        ),
                      ),

                      Positioned.fill(
                        child: CustomPaint(
                          painter: CornerBracketsPainter(
                            color: accent.withOpacity(0.5),
                            size: 32,
                          ),
                        ),
                      ),

                      Positioned(
                        top: 0,
                        left: 0,
                        right: 0,
                        child: _TopBar(
                          accent: accent,
                          linkState: s.linkState,
                          demoMode: demoActive,
                        ),
                      ),

                      Positioned(
                        bottom: 16,
                        left: 16,
                        right: 16,
                        child: isWide
                            ? _WideBottomRow(state: s)
                            : _NarrowBottomColumn(state: s),
                      ),
                    ],
                  );
                },
              ),
            ],
          );
        },
      ),
    );
  }
}

class _TopBar extends StatelessWidget {
  final Color accent;
  final LinkState linkState;
  final bool demoMode;

  const _TopBar(
      {required this.accent,
      required this.linkState,
      required this.demoMode});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 10),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          colors: [
            AegisColors.background.withOpacity(0.95),
            AegisColors.background.withOpacity(0.0),
          ],
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
        ),
      ),
      child: Row(
        children: [
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                'AEGIS EDGE',
                style: GoogleFonts.exo2(
                  color: accent,
                  fontSize: 16,
                  fontWeight: FontWeight.w800,
                  letterSpacing: 4,
                ),
              ),
              Text(
                'TACTICAL HUD  v1.0',
                style: GoogleFonts.exo2(
                  color: AegisColors.textDim,
                  fontSize: 9,
                  letterSpacing: 2.5,
                ),
              ),
            ],
          ),

          const Spacer(),

          StatusBadge(linkState: linkState, demoMode: demoMode),

          const Spacer(),

          Column(
            crossAxisAlignment: CrossAxisAlignment.end,
            children: [
              Text(
                'DRONE-01',
                style: GoogleFonts.exo2(
                  color: AegisColors.textSecondary,
                  fontSize: 12,
                  letterSpacing: 2,
                ),
              ),
              _ClockWidget(),
            ],
          ),
        ],
      ),
    );
  }
}

class _ClockWidget extends StatefulWidget {
  @override
  State<_ClockWidget> createState() => _ClockWidgetState();
}

class _ClockWidgetState extends State<_ClockWidget> {
  String _time = '';
  late final StreamSubscription<void> _ticker;

  @override
  void initState() {
    super.initState();
    _updateTime();
    _ticker = Stream.periodic(const Duration(seconds: 1)).listen((_) {
      if (mounted) setState(_updateTime);
    });
  }

  @override
  void dispose() {
    _ticker.cancel();
    super.dispose();
  }

  void _updateTime() {
    final now = DateTime.now();
    _time =
        '${now.hour.toString().padLeft(2, '0')}:${now.minute.toString().padLeft(2, '0')}:${now.second.toString().padLeft(2, '0')}Z';
  }

  @override
  Widget build(BuildContext context) {
    return Text(
      _time,
      style: GoogleFonts.exo2(
        color: AegisColors.textDim,
        fontSize: 9,
        letterSpacing: 2,
      ),
    );
  }
}

class _WideBottomRow extends StatelessWidget {
  final DroneState state;
  const _WideBottomRow({required this.state});

  @override
  Widget build(BuildContext context) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.end,
      children: [
        MiniMap(state: state),
        const Spacer(),
        ArtificialHorizonWidget(state: state),
        const SizedBox(width: 12),
        TelemetryPanel(state: state),
      ],
    );
  }
}

class _NarrowBottomColumn extends StatelessWidget {
  final DroneState state;
  const _NarrowBottomColumn({required this.state});

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        TelemetryPanel(state: state),
        const SizedBox(height: 8),
        Row(
          children: [
            Expanded(child: ArtificialHorizonWidget(state: state)),
            const SizedBox(width: 8),
            Expanded(child: MiniMap(state: state)),
          ],
        ),
      ],
    );
  }
}
