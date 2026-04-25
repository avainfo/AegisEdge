class HorizonPoint {
  final double x;
  final double y;

  const HorizonPoint({required this.x, required this.y});

  factory HorizonPoint.fromJson(Map<String, dynamic> json) => HorizonPoint(
        x: (json['x'] as num).toDouble(),
        y: (json['y'] as num).toDouble(),
      );
}

class HorizonData {
  final bool detected;
  final List<HorizonPoint> points;
  final double confidence;

  const HorizonData({
    required this.detected,
    required this.points,
    required this.confidence,
  });

  factory HorizonData.empty() => const HorizonData(
        detected: false,
        points: [],
        confidence: 0.0,
      );

  factory HorizonData.fromJson(Map<String, dynamic> json) {
    final rawPoints = json['points'] as List<dynamic>? ?? [];
    return HorizonData(
      detected: json['detected'] as bool? ?? false,
      confidence: (json['confidence'] as num?)?.toDouble() ?? 0.0,
      points: rawPoints
          .map((p) => HorizonPoint.fromJson(p as Map<String, dynamic>))
          .toList(),
    );
  }
}
