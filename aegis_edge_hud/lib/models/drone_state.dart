import 'horizon_data.dart';
import 'link_state.dart';

class DroneState {
  final int frameId;
  final int timestampMs;
  final LinkState linkState;
  final int latencyMs;
  final bool stale;
  final bool sourceValid;
  final int lastUpdateMs;
  final double posX;
  final double posY;
  final double roll;
  final double pitch;
  final double yaw;
  final double altitude;
  final HorizonData horizon;

  // Video Control Fields
  final bool frameAvailable;
  final String frameEndpoint;
  final String frameMime;
  final String frameTransport;
  final String videoState;
  final bool videoStale;
  final bool videoShouldFetch;
  final bool videoShouldFreeze;

  const DroneState({
    required this.frameId,
    required this.timestampMs,
    required this.linkState,
    required this.latencyMs,
    required this.stale,
    required this.sourceValid,
    required this.lastUpdateMs,
    required this.posX,
    required this.posY,
    required this.roll,
    required this.pitch,
    required this.yaw,
    required this.altitude,
    required this.horizon,
    required this.frameAvailable,
    required this.frameEndpoint,
    required this.frameMime,
    required this.frameTransport,
    required this.videoState,
    required this.videoStale,
    required this.videoShouldFetch,
    required this.videoShouldFreeze,
  });

  factory DroneState.initial() => DroneState(
        frameId: 0,
        timestampMs: 0,
        linkState: LinkState.lost,
        latencyMs: 0,
        stale: true,
        sourceValid: false,
        lastUpdateMs: 0,
        posX: 0,
        posY: 0,
        roll: 0,
        pitch: 0,
        yaw: 0,
        altitude: 0,
        horizon: HorizonData.empty(),
        frameAvailable: false,
        frameEndpoint: "http://127.0.0.1:8080/snapshot",
        frameMime: "image/png",
        frameTransport: "HTTP_SNAPSHOT",
        videoState: "UNAVAILABLE",
        videoStale: true,
        videoShouldFetch: false,
        videoShouldFreeze: false,
      );

  factory DroneState.fromJson(Map<String, dynamic> json) {
    return DroneState(
      frameId: (json['frame_id'] as num?)?.toInt() ?? 0,
      timestampMs: (json['timestamp_ms'] as num?)?.toInt() ?? 0,
      linkState: LinkState.fromString(json['link_state'] as String? ?? 'LOST'),
      latencyMs: (json['latency_ms'] as num?)?.toInt() ?? 0,
      stale: json['stale'] as bool? ?? true,
      sourceValid: json['source_valid'] as bool? ?? false,
      lastUpdateMs: (json['last_update_ms'] as num?)?.toInt() ?? 0,
      posX: (json['position']?['x'] as num?)?.toDouble() ?? 0.0,
      posY: (json['position']?['y'] as num?)?.toDouble() ?? 0.0,
      roll: (json['roll'] as num?)?.toDouble() ?? 0.0,
      pitch: (json['pitch'] as num?)?.toDouble() ?? 0.0,
      yaw: (json['yaw'] as num?)?.toDouble() ?? 0.0,
      altitude: (json['altitude'] as num?)?.toDouble() ?? 0.0,
      horizon: HorizonData.fromJson(
          json['horizon'] as Map<String, dynamic>? ?? {}),
      frameAvailable: json['frame_available'] as bool? ?? false,
      frameEndpoint: json['frame_endpoint'] as String? ?? "http://127.0.0.1:8080/snapshot",
      frameMime: json['frame_mime'] as String? ?? "image/png",
      frameTransport: json['frame_transport'] as String? ?? "HTTP_SNAPSHOT",
      videoState: json['video_state'] as String? ?? "UNAVAILABLE",
      videoStale: json['video_stale'] as bool? ?? true,
      videoShouldFetch: json['video_should_fetch'] as bool? ?? false,
      videoShouldFreeze: json['video_should_freeze'] as bool? ?? false,
    );
  }
}
