#include "json_utils.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

ParseResult parseRawJson(const std::string& jsonString) {
    ParseResult result;
    result.success = false;

    try {
        auto j = json::parse(jsonString);

        result.timestamp_ms = j.value("timestamp_ms", 0LL);
        
        if (j.contains("position")) {
            result.data.posX = j["position"].value("x", 0.0);
            result.data.posY = j["position"].value("y", 0.0);
        }

        result.data.roll = j.value("roll", 0.0);
        result.data.pitch = j.value("pitch", 0.0);
        result.data.yaw = j.value("yaw", 0.0);
        result.data.altitude = j.value("altitude", 0.0);

        if (j.contains("horizon")) {
            auto hz = j["horizon"];
            result.data.horizon.detected = hz.value("detected", false);
            result.data.horizon.confidence = hz.value("confidence", 0.0);
            
            if (hz.contains("points") && hz["points"].is_array()) {
                for (const auto& pt : hz["points"]) {
                    result.data.horizon.points.push_back({
                        pt.value("x", 0.0),
                        pt.value("y", 0.0)
                    });
                }
            }
        }
        
        result.success = true;
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
    }
    
    return result;
}

std::string serializeEnrichedJson(const TelemetryProcessor& processor) {
    const auto& tel = processor.getLastValidTelemetry();
    
    json j;
    j["link_state"] = LinkStateToString(processor.getLinkState());
    j["latency_ms"] = processor.getLatency();
    j["stale"] = processor.isStale();
    j["source_valid"] = processor.isSourceValid();
    j["last_update_ms"] = processor.getLastUpdateMs();
    
    j["position"] = { {"x", tel.posX}, {"y", tel.posY} };
    j["roll"] = tel.roll;
    j["pitch"] = tel.pitch;
    j["yaw"] = tel.yaw;
    j["altitude"] = tel.altitude;
    
    json hz;
    hz["detected"] = tel.horizon.detected;
    hz["confidence"] = tel.horizon.confidence;
    
    json points = json::array();
    for (const auto& p : tel.horizon.points) {
        points.push_back({{"x", p.x}, {"y", p.y}});
    }
    hz["points"] = points;
    
    j["horizon"] = hz;
    
    return j.dump();
}
