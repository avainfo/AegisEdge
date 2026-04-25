enum LinkState {
  normal,
  degraded,
  lost;

  static LinkState fromString(String s) => switch (s.toUpperCase()) {
        'NORMAL' => LinkState.normal,
        'DEGRADED' => LinkState.degraded,
        _ => LinkState.lost,
      };

  String get displayLabel => switch (this) {
        LinkState.normal => '● LIVE',
        LinkState.degraded => '⚠ DEGRADED DATA',
        LinkState.lost => '✕ SIGNAL LOST',
      };

  String get shortLabel => switch (this) {
        LinkState.normal => 'NORMAL',
        LinkState.degraded => 'DEGRADED',
        LinkState.lost => 'LOST',
      };
}
