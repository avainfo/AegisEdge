#include "telemetry.hpp"
#include <chrono>

TelemetryProcessor::TelemetryProcessor() {
    lastValidTelemetry = TelemetryData();
}

int64_t TelemetryProcessor::getCurrentTimeMs() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               now.time_since_epoch())
        .count();
}

void TelemetryProcessor::updateWithPacket(int64_t packetTimestampMs, const TelemetryData& data) {
    int64_t currentMs = getCurrentTimeMs();
    
    // We assume the simulator timestamp is system epoch ms.
    // If packetTimestampMs is way off, we fallback to currentMs to measure latency
    int64_t latency = currentMs - packetTimestampMs;
    if (latency < 0 || latency > 1000000) {
        // Fallback for relative timestamps
        latency = 0; 
    }
    
    lastPacketReceivedTimeMs = currentMs;
    lastReportedLatencyMs = latency;
    
    // Save valid telemetry
    lastValidTelemetry = data;

    // Update state machine
    if (latency < 300) {
        currentState = LinkState::NORMAL;
    } else if (latency < 2000) {
        currentState = LinkState::DEGRADED;
    } else {
        currentState = LinkState::LOST;
    }
}

void TelemetryProcessor::updateStateWithoutPacket() {
    if (lastPacketReceivedTimeMs == 0) {
        currentState = LinkState::LOST;
        return;
    }

    int64_t currentMs = getCurrentTimeMs();
    int64_t elapsedSinceLast = currentMs - lastPacketReceivedTimeMs;

    if (elapsedSinceLast >= 2000) {
        currentState = LinkState::LOST;
    } else if (elapsedSinceLast >= 300) {
        // Only degrade if we were NORMAL. If we are already DEGRADED, stay DEGRADED.
        if (currentState == LinkState::NORMAL) {
            currentState = LinkState::DEGRADED;
        }
    }
    
    // Increase reported latency based on time since last packet if we haven't received one
    if (elapsedSinceLast > lastReportedLatencyMs) {
        lastReportedLatencyMs = elapsedSinceLast;
    }
}

LinkState TelemetryProcessor::getLinkState() const {
    return currentState;
}

int64_t TelemetryProcessor::getLatency() const {
    return lastReportedLatencyMs;
}

bool TelemetryProcessor::isStale() const {
    return currentState == LinkState::LOST || currentState == LinkState::DEGRADED;
}

bool TelemetryProcessor::isSourceValid() const {
    return currentState != LinkState::LOST;
}

int64_t TelemetryProcessor::getLastUpdateMs() const {
    return lastPacketReceivedTimeMs;
}

int64_t TelemetryProcessor::getTimeSinceLastPacketMs() const {
    if (lastPacketReceivedTimeMs == 0) return 0;
    return getCurrentTimeMs() - lastPacketReceivedTimeMs;
}

const TelemetryData& TelemetryProcessor::getLastValidTelemetry() const {
    return lastValidTelemetry;
}
