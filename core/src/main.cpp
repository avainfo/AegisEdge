#include "udp_socket.hpp"
#include "telemetry.hpp"
#include "json_utils.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "Starting Aegis Edge Core Backend..." << std::endl;

    UdpSocket socket;
    if (!socket.bind(6000)) {
        std::cerr << "Failed to bind to 6000" << std::endl;
        return 1;
    }
    
    // Crucial: non-blocking receive loop
    socket.setNonBlocking(true);

    TelemetryProcessor processor;
    char buffer[8192];
    
    auto nextSendTime = std::chrono::steady_clock::now();
    const auto sendInterval = std::chrono::milliseconds(100);

    LinkState lastLoggedState = LinkState::LOST;

    while (true) {
        int n = socket.receive(buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            ParseResult res = parseRawJson(buffer);
            if (res.success) {
                processor.updateWithPacket(res.timestamp_ms, res.data);
                std::cout << "[PACKET] Latency: " << processor.getLatency() 
                          << "ms | Points: " << res.data.horizon.points.size() << std::endl;
            }
        }

        // Always update state to handle packet loss correctly
        processor.updateStateWithoutPacket();

        // Log state transitions
        if (processor.getLinkState() != lastLoggedState) {
            std::cout << "[STATE] Transition: " << LinkStateToString(lastLoggedState) 
                      << " -> " << LinkStateToString(processor.getLinkState()) << std::endl;
            lastLoggedState = processor.getLinkState();
        }

        auto now = std::chrono::steady_clock::now();
        if (now >= nextSendTime) {
            // Serialize and send enriched JSON to Flutter every 100ms
            std::string outJson = serializeEnrichedJson(processor);
            socket.sendTo("127.0.0.1", 5005, outJson);
            
            nextSendTime += sendInterval;
        }

        // Sleep 10ms to avoid busy-waiting 100% CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}
