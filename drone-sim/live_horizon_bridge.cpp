#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include "../DetectionSystems/horizon.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

using json = nlohmann::json;

std::string simpleHttpGet(const std::string& host, int port, const std::string& path) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(host.c_str());

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return "";
    }

    std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
    send(sock, request.c_str(), request.size(), 0);

    std::string response;
    char buffer[4096];
    int bytes_received;
    while ((bytes_received = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, bytes_received);
    }
    close(sock);

    size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos) return "";
    return response.substr(header_end + 4);
}

int main() {
    std::cout << "[LIVE-HORIZON] Starting Optimized Live Bridge (Socket Mode)..." << std::endl;

    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(5000);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    OptimizedHorizonDetector detector;
    
    auto last_fps_time = std::chrono::steady_clock::now();
    int frame_count = 0;

    while (true) {
        auto frame_start = std::chrono::steady_clock::now();

        std::string tel_data = simpleHttpGet("127.0.0.1", 8081, "/telemetry");
        if (tel_data.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        json tel_json;
        try {
            tel_json = json::parse(tel_data);
        } catch (...) {
            continue;
        }

        ImuData imu;
        imu.roll = tel_json.value("roll", 0.0);
        imu.pitch = tel_json.value("pitch", 0.0);
        detector.updateImu(imu);

        std::string img_data = simpleHttpGet("127.0.0.1", 8081, "/snapshot");
        if (img_data.empty()) continue;

        std::vector<uchar> bytes(img_data.begin(), img_data.end());
        cv::Mat frame = cv::imdecode(bytes, cv::IMREAD_COLOR);
        if (frame.empty()) continue;

        HorizonLine result = detector.process(frame);

        // 6. Convert to normalized points
        double width = frame.cols;
        double height = frame.rows;
        
        std::vector<json> points;
        double x_positions[] = {0.05, 0.50, 0.95};
        
        for (double x_norm : x_positions) {
            double x_px = x_norm * width;
            double center_y = height / 2.0 + result.offset;
            double angle_rad = result.angle * M_PI / 180.0;
            double y_px = center_y + (x_px - width / 2.0) * std::tan(angle_rad);
            
            double y_norm = std::max(0.0, std::min(1.0, y_px / height));
            points.push_back({{"x", x_norm}, {"y", y_norm}});
        }

        json output = tel_json;
        output["horizon"]["detected"] = true;
        output["horizon"]["points"] = points;
        output["horizon"]["confidence"] = result.confidence;
        output["horizon"]["estimated"] = result.is_estimated;
        
        output["frame"]["endpoint"] = "http://127.0.0.1:8080/snapshot";
        output["frame"]["transport"] = "HTTP_SNAPSHOT_PROXY";

        std::string payload = output.dump();
        sendto(udp_sock, payload.c_str(), payload.size(), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));

        frame_count++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed_fps = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time).count();
        if (elapsed_fps >= 2000) {
            double fps = frame_count * 1000.0 / elapsed_fps;
            std::cout << "[LIVE-HORIZON] " 
                      << (result.is_estimated ? "IMU ONLY" : "IMU + Hough")
                      << " fps=" << std::fixed << std::setprecision(1) << fps 
                      << " conf=" << result.confidence << std::endl;
            frame_count = 0;
            last_fps_time = now;
        }

        auto frame_end = std::chrono::steady_clock::now();
        auto process_time = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();
        int sleep_ms = std::max(0, 50 - (int)process_time);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }

    close(udp_sock);
    return 0;
}
