#pragma once

#include "telemetry.hpp"
#include <string>

// Forward declare to avoid exposing json.hpp in headers if we wanted to
// But we'll just parse strings
struct ParseResult {
    bool success;
    int64_t timestamp_ms;
    TelemetryData data;
};

ParseResult parseRawJson(const std::string& jsonString);
std::string serializeEnrichedJson(const TelemetryProcessor& processor);
