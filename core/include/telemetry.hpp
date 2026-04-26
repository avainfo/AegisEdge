#pragma once

#include "link_state.hpp"
#include <vector>
#include <cstdint>

struct Point2D {
    double x;
    double y;
};

struct HorizonData {
    bool detected = false;
    double confidence = 0.0;
    std::vector<Point2D> points;
};

struct TelemetryData {
    int64_t frame_id = 0;
    int64_t timestamp_ms = 0;
    double posX = 0.0;
    double posY = 0.0;
    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
    double altitude = 0.0;
    HorizonData horizon;
};

class TelemetryProcessor {
public:
    TelemetryProcessor();

    // Process an incoming packet
    void updateWithPacket(int64_t packetTimestampMs, const TelemetryData& data);
    
    // Call periodically to update state when no packets arrive
    void updateStateWithoutPacket();

    LinkState getLinkState() const;
    int64_t getLatency() const;
    bool isStale() const;
    bool isSourceValid() const;
    int64_t getLastUpdateMs() const;
    int64_t getTimeSinceLastPacketMs() const;
    const TelemetryData& getLastValidTelemetry() const;

private:
    int64_t getCurrentTimeMs() const;

    int64_t lastPacketReceivedTimeMs = 0;
    int64_t lastReportedLatencyMs = 0;
    
    LinkState currentState = LinkState::LOST;
    TelemetryData lastValidTelemetry;
};
