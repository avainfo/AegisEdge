import 'package:collection/collection.dart';

class HorizonPoint {
  final double x;
  final double y;

  const HorizonPoint({required this.x, required this.y});

  factory HorizonPoint.fromJson(Map<String, dynamic> json) => HorizonPoint(
        x: (json['x'] as num).toDouble(),
        y: (json['y'] as num).toDouble(),
      );

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is HorizonPoint && other.x == x && other.y == y;

  @override
  int get hashCode => Object.hash(x, y);
}

class HorizonData {
  final bool detected;
  final List<HorizonPoint> points;
  final double confidence;
  final bool estimated;
  final String source;

  const HorizonData({
    required this.detected,
    required this.points,
    required this.confidence,
    required this.estimated,
    required this.source,
  });

  factory HorizonData.empty() => const HorizonData(
        detected: false,
        points: [],
        confidence: 0.0,
        estimated: true,
        source: 'UNKNOWN',
      );

  factory HorizonData.fromJson(Map<String, dynamic> json) {
    final rawPoints = json['points'] as List<dynamic>? ?? [];
    return HorizonData(
      detected: json['detected'] as bool? ?? false,
      confidence: (json['confidence'] as num?)?.toDouble() ?? 0.0,
      estimated: json['estimated'] as bool? ?? true,
      source: json['source'] as String? ?? 'UNKNOWN',
      points: rawPoints
          .map((p) => HorizonPoint.fromJson(p as Map<String, dynamic>))
          .toList(),
    );
  }

  static const _listEq = ListEquality<HorizonPoint>();

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is HorizonData &&
          other.detected == detected &&
          other.confidence == confidence &&
          other.estimated == estimated &&
          other.source == source &&
          _listEq.equals(other.points, points);

  @override
  int get hashCode => Object.hash(
      detected, confidence, estimated, source, Object.hashAll(points));
}
