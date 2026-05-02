#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <memory>
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

int main(int argc, char* argv[]) {
    bool use_debug_window = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--debug-window") {
            use_debug_window = true;
        }
    }

    std::cout << "[LIVE-HORIZON] Starting Optimized Live Bridge" << std::endl;
    std::cout << "[LIVE-HORIZON] Snapshot source: http://127.0.0.1:8081/snapshot" << std::endl;
    std::cout << "[LIVE-HORIZON] Telemetry source: http://127.0.0.1:8081/telemetry" << std::endl;
    std::cout << "[LIVE-HORIZON] UDP output: 127.0.0.1:5000" << std::endl;
    if (use_debug_window) std::cout << "[LIVE-HORIZON] Debug window ENABLED" << std::endl;

    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(5000);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    std::unique_ptr<OptimizedHorizonDetector> detector;
    int detector_width = 0;
    int detector_height = 0;
    
    auto last_fps_time = std::chrono::steady_clock::now();
    int frame_count = 0;
    int error_count = 0;

    while (true) {
        auto frame_start = std::chrono::steady_clock::now();

        std::string tel_data = simpleHttpGet("127.0.0.1", 8081, "/telemetry");
        json tel_json;
        bool telemetry_valid = false;
        
        if (!tel_data.empty()) {
            try {
                tel_json = json::parse(tel_data);
                telemetry_valid = true;
            } catch (...) {
                error_count++;
                if (error_count % 100 == 0) std::cerr << "[LIVE-HORIZON] Telemetry JSON parse failed" << std::endl;
            }
        } else {
            error_count++;
        }

        double roll = tel_json.value("roll", 0.0);
        double pitch = tel_json.value("pitch", 0.0);
        
        std::string img_data = simpleHttpGet("127.0.0.1", 8081, "/snapshot");
        if (img_data.empty()) {
            error_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }

        std::vector<uchar> bytes(img_data.begin(), img_data.end());
        cv::Mat frame = cv::imdecode(bytes, cv::IMREAD_COLOR);
        if (frame.empty() || frame.cols == 0 || frame.rows == 0) {
            error_count++;
            continue;
        }

        if (!detector || detector_width != frame.cols || detector_height != frame.rows) {
            DetectorConfig cfg;
            cfg.frame_width = frame.cols;
            cfg.frame_height = frame.rows;
            cfg.v_fov_degrees = 90.0f;

            cfg.roi_height_base = std::clamp(
                static_cast<int>(frame.rows * 0.45f),
                96,
                frame.rows
            );

            cfg.max_seconds_no_hailo = 1.5f;

            cfg.max_hough_angle_error_deg = 4.0f;
            cfg.max_hough_y_error_ratio = 0.15f;
            cfg.max_hough_offset_correction_px = 12.0f;
            cfg.min_hough_confidence = 0.80f;
            cfg.min_sky_ground_contrast = 14.0f;

            cfg.imu_angle_smoothing_alpha = 0.35f;

            detector = std::make_unique<OptimizedHorizonDetector>(cfg);
            detector_width = frame.cols;
            detector_height = frame.rows;

            std::cout << "[LIVE-HORIZON] Initializing detector for frame size "
                      << detector_width << "x" << detector_height << std::endl;
            
            std::cout << "[LIVE-HORIZON] Detector config: "
                      << "roi_height_base=" << cfg.roi_height_base
                      << " max_seconds_no_hailo=" << cfg.max_seconds_no_hailo
                      << " max_angle_err=" << cfg.max_hough_angle_error_deg
                      << " max_y_err_ratio=" << cfg.max_hough_y_error_ratio
                      << " max_offset_correction=" << cfg.max_hough_offset_correction_px
                      << " min_conf=" << cfg.min_hough_confidence
                      << " min_contrast=" << cfg.min_sky_ground_contrast
                      << " imu_alpha=" << cfg.imu_angle_smoothing_alpha
                      << std::endl;

            if (!detector->init("model.hef")) {
                std::cerr << "[LIVE-HORIZON] Detector init failed, continuing with fallback if available" << std::endl;
            }
        }

        ImuData imu;
        imu.roll = roll;
        imu.pitch = pitch;
        detector->updateImu(imu);

        HorizonLine result = detector->process(frame);

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

        json output;
        if (telemetry_valid) {
            output = tel_json;
        } else {
            output = {
                {"frame_id", 0},
                {"timestamp_ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()},
                {"roll", roll},
                {"pitch", pitch},
                {"yaw", 0.0},
                {"altitude", 0.0},
                {"position", {{"x", 0.0}, {"y", 0.0}}}
            };
        }

        output["horizon"]["detected"] = true;
        output["horizon"]["points"] = points;
        output["horizon"]["confidence"] = result.confidence;
        output["horizon"]["estimated"] = result.is_estimated;
        output["horizon"]["source"] = result.source;
        
        output["frame"]["available"] = true;
        output["frame"]["endpoint"] = "http://127.0.0.1:8080/snapshot";
        output["frame"]["mime"] = "image/png";
        output["frame"]["transport"] = "HTTP_SNAPSHOT_PROXY";

        // Optional Debug Visualization
        if (use_debug_window) {
            cv::Mat debug_img = frame.clone();
            cv::Point p1(0, height/2 + result.offset - (width/2) * std::tan(result.angle * M_PI / 180.0));
            cv::Point p2(width, height/2 + result.offset + (width/2) * std::tan(result.angle * M_PI / 180.0));
            cv::line(debug_img, p1, p2, cv::Scalar(0, 255, 0), 2);
            
            std::stringstream ss;
            ss << std::fixed << std::setprecision(1);
            ss << result.source << " conf=" << std::setprecision(2) << result.confidence;
            cv::putText(debug_img, ss.str(), cv::Point(20, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
            
            ss.str(""); ss.clear();
            ss << "angle=" << result.angle << " exp=" << result.expected_angle << " err=" << result.angle_error;
            cv::putText(debug_img, ss.str(), cv::Point(20, 60), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
            
            cv::imshow("AegisEdge Debug", debug_img);
            if (cv::waitKey(1) == 'q') {
                std::cout << "[LIVE-HORIZON] Closing debug window..." << std::endl;
                use_debug_window = false;
                cv::destroyAllWindows();
            }
        }

        std::string payload = output.dump();
        sendto(udp_sock, payload.c_str(), payload.size(), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));

        frame_count++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed_fps = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time).count();
        if (elapsed_fps >= 2000) {
            double fps = frame_count * 1000.0 / elapsed_fps;
            std::cout << "[LIVE-HORIZON] mode=" << detector->visionModeName()
                      << " fps=" << std::fixed << std::setprecision(1) << fps 
                      << " source=" << result.source
                      << " angle=" << std::setprecision(1) << result.angle
                      << " expected=" << result.expected_angle
                      << " err=" << result.angle_error
                      << " offset=" << std::setprecision(1) << result.offset
                      << " conf=" << std::setprecision(2) << result.confidence 
                      << " r=" << std::setprecision(1) << roll 
                      << " p=" << std::setprecision(1) << pitch 
                      << " size=" << detector_width << "x" << detector_height << std::endl;
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
