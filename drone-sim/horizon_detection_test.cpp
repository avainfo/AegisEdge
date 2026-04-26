// horizon_detection_test.cpp
// AegisEdge - Horizon Detection Test
//
// Usage:
//   ./horizon_detection_test [csv] [frames_dir] [--window 0|1] [--udp IP:PORT] [--jsonl output.jsonl]
//
// Build (WITH_HOUGH recommended for demo without Hailo hardware):
//   mkdir -p build && cd build
//   cmake .. -DWITH_HOUGH=ON
//   cmake --build .
//
// Example E2E pipeline:
//   ./horizon_detection_test --udp 127.0.0.1:5000

#include "../DetectionSystems/horizon.hpp"
#include <opencv2/opencv.hpp>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <climits>
#include <cmath>
#include <chrono>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Telemetry CSV entry
// ---------------------------------------------------------------------------
struct TelemetryEntry {
    long long timestamp_ms;
    double    roll;
    double    pitch;
    double    yaw       = 0.0;
    double    altitude  = 42.5;
    double    pos_x     = 12.4;
    double    pos_y     = 8.7;
    std::string drone_id = "DRONE_01";
};

std::vector<TelemetryEntry> loadTelemetry(const std::string& path) {
    std::vector<TelemetryEntry> entries;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[CSV] Cannot open: " << path << "\n";
        return entries;
    }

    std::string line;
    std::getline(file, line); // skip header

    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string token;
        TelemetryEntry e;

        // Expected format: timestamp_ms,drone_id,x,y,roll,pitch,yaw,altitude
        std::getline(ss, token, ','); e.timestamp_ms = std::stoll(token);
        std::getline(ss, token, ','); e.drone_id = token;
        std::getline(ss, token, ','); e.pos_x  = std::stod(token);
        std::getline(ss, token, ','); e.pos_y  = std::stod(token);
        std::getline(ss, token, ','); e.roll   = std::stod(token);
        std::getline(ss, token, ','); e.pitch  = std::stod(token);
        if (std::getline(ss, token, ',')) e.yaw      = std::stod(token);
        if (std::getline(ss, token, ',')) e.altitude = std::stod(token);

        entries.push_back(e);
    }
    std::cout << "[CSV] " << entries.size() << " telemetry entries loaded.\n";
    return entries;
}

// ---------------------------------------------------------------------------
// Frame entries
// ---------------------------------------------------------------------------
struct FrameEntry {
    long long   timestamp_ns;
    std::string path;
};

std::vector<FrameEntry> loadFrames(const std::string& dir) {
    std::vector<FrameEntry> frames;
    if (!fs::exists(dir)) {
        std::cerr << "[Frames] Directory not found: " << dir << "\n";
        return frames;
    }

    for (const auto& entry : fs::directory_iterator(dir)) {
        std::string name = entry.path().filename().string();
        auto pos = name.rfind('_');
        auto dot = name.rfind('.');
        if (pos == std::string::npos || dot == std::string::npos) continue;
        try {
            long long ts = std::stoll(name.substr(pos + 1, dot - pos - 1));
            frames.push_back({ts, entry.path().string()});
        } catch (...) {}
    }
    std::sort(frames.begin(), frames.end(),
              [](const FrameEntry& a, const FrameEntry& b) {
                  return a.timestamp_ns < b.timestamp_ns;
              });
    std::cout << "[Frames] " << frames.size() << " frames found.\n";
    return frames;
}

const FrameEntry* findClosestFrame(const std::vector<FrameEntry>& frames,
                                   long long target_ms) {
    long long target_ns = target_ms * 1000000LL;
    const FrameEntry* best = nullptr;
    long long best_diff = LLONG_MAX;
    for (const auto& f : frames) {
        long long diff = std::llabs(f.timestamp_ns - target_ns);
        if (diff < best_diff) {
            best_diff = diff;
            best = &f;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Convert HorizonLine to normalized JSON-compatible points
// Returns 3 points at x = 0.05, 0.50, 0.95
// ---------------------------------------------------------------------------
struct NormalizedPoint { double x, y; };

std::vector<NormalizedPoint> horizonToPoints(
        const HorizonLine& h, int width, int height) {
    double center_y  = height / 2.0 + h.offset;
    double angle_rad = h.angle * M_PI / 180.0;

    std::vector<double> xs_norm = {0.05, 0.50, 0.95};
    std::vector<NormalizedPoint> pts;

    for (double x_norm : xs_norm) {
        double x_px = x_norm * width;
        double y    = center_y + std::tan(angle_rad) * (x_px - width / 2.0);
        double y_norm = std::clamp(y / height, 0.0, 1.0);
        pts.push_back({x_norm, y_norm});
    }
    return pts;
}

// ---------------------------------------------------------------------------
// Serialise one frame result to JSON string (compatible with core parser)
// ---------------------------------------------------------------------------
std::string toJson(const TelemetryEntry& tel,
                   const HorizonLine& h,
                   const std::vector<NormalizedPoint>& pts,
                   bool detected) {
    std::ostringstream j;
    j << std::fixed << std::setprecision(4);

    j << "{";
    j << "\"timestamp_ms\":" << tel.timestamp_ms << ",";
    j << "\"drone_id\":\"" << tel.drone_id << "\",";
    j << "\"position\":{\"x\":" << tel.pos_x << ",\"y\":" << tel.pos_y << "},";
    j << "\"roll\":"     << tel.roll     << ",";
    j << "\"pitch\":"    << tel.pitch    << ",";
    j << "\"yaw\":"      << tel.yaw      << ",";
    j << "\"altitude\":" << tel.altitude << ",";
    j << "\"image\":{\"width\":1920,\"height\":1080},";
    j << "\"horizon\":{";

    if (!detected) {
        j << "\"detected\":false,"
          << "\"type\":\"NONE\","
          << "\"points\":[],"
          << "\"ground_side\":\"UNKNOWN\","
          << "\"confidence\":0.0";
    } else {
        j << "\"detected\":true,";
        j << "\"type\":\"POLYLINE\",";
        j << "\"points\":[";
        for (size_t i = 0; i < pts.size(); ++i) {
            j << "{\"x\":" << pts[i].x << ",\"y\":" << pts[i].y << "}";
            if (i + 1 < pts.size()) j << ",";
        }
        j << "],";
        j << "\"ground_side\":\"BELOW\",";
        j << "\"confidence\":" << h.confidence;
        if (h.is_estimated) j << ",\"estimated\":true";
    }
    j << "}}";
    return j.str();
}

// ---------------------------------------------------------------------------
// Simple POSIX UDP sender
// ---------------------------------------------------------------------------
class UdpSender {
public:
    UdpSender() : sockfd_(-1) {}
    ~UdpSender() { if (sockfd_ >= 0) close(sockfd_); }

    bool init(const std::string& ip, int port) {
        sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd_ < 0) return false;

        memset(&dest_, 0, sizeof(dest_));
        dest_.sin_family = AF_INET;
        dest_.sin_port   = htons(port);
        inet_pton(AF_INET, ip.c_str(), &dest_.sin_addr);
        return true;
    }

    void send(const std::string& data) {
        if (sockfd_ < 0) return;
        sendto(sockfd_, data.c_str(), data.size(), 0,
               (const struct sockaddr*)&dest_, sizeof(dest_));
    }

private:
    int sockfd_;
    struct sockaddr_in dest_{};
};

// ---------------------------------------------------------------------------
// Draw horizon line overlay on frame
// ---------------------------------------------------------------------------
void drawHorizon(cv::Mat& frame, const HorizonLine& h) {
    const int W = frame.cols, H = frame.rows;
    float center_y  = H / 2.0f + h.offset;
    float angle_rad = h.angle * static_cast<float>(CV_PI) / 180.0f;
    float dx = (W / 2.0f) * std::cos(angle_rad);
    float dy = (W / 2.0f) * std::sin(angle_rad);

    cv::Point p1(static_cast<int>(W / 2.0f - dx), static_cast<int>(center_y - dy));
    cv::Point p2(static_cast<int>(W / 2.0f + dx), static_cast<int>(center_y + dy));

    cv::Scalar color = h.is_estimated ? cv::Scalar(0, 200, 255) : cv::Scalar(0, 255, 0);
    cv::line(frame, p1, p2, color, 2, cv::LINE_AA);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1)
        << "angle: " << h.angle << " deg  "
        << "offset: " << h.offset << " px  "
        << "conf: " << std::setprecision(2) << h.confidence
        << (h.is_estimated ? "  [IMU]" : "  [VISION]");
    cv::putText(frame, oss.str(), {10, 30},
                cv::FONT_HERSHEY_SIMPLEX, 0.7, color, 2);
}

// ---------------------------------------------------------------------------
// Parse --udp IP:PORT argument
// ---------------------------------------------------------------------------
bool parseUdpArg(const std::string& arg, std::string& ip, int& port) {
    auto colon = arg.rfind(':');
    if (colon == std::string::npos) return false;
    ip   = arg.substr(0, colon);
    port = std::stoi(arg.substr(colon + 1));
    return true;
}

// ---------------------------------------------------------------------------
// main()
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    std::string csv_path   = "../Assets/logs/airsim_telemetry_log.csv";
    std::string frames_dir = "../Assets/images/images";
    bool        show_window = true;
    std::string udp_target  = "";
    std::string jsonl_path  = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--udp" && i + 1 < argc) {
            udp_target = argv[++i];
        } else if (arg == "--jsonl" && i + 1 < argc) {
            jsonl_path = argv[++i];
        } else if (arg == "--window" && i + 1 < argc) {
            show_window = std::string(argv[++i]) != "0";
        } else if (arg == "--no-window") {
            show_window = false;
        } else if (arg[0] != '-') {
            if (csv_path == "../Assets/logs/airsim_telemetry_log.csv")
                csv_path = arg;
            else
                frames_dir = arg;
        }
    }

    std::cout << "[AegisEdge] Horizon Detection Test\n";
    std::cout << "[AegisEdge] Mode: " << OptimizedHorizonDetector::visionModeName() << "\n";
    if (!udp_target.empty())  std::cout << "[AegisEdge] UDP -> " << udp_target << "\n";
    if (!jsonl_path.empty())  std::cout << "[AegisEdge] JSONL -> " << jsonl_path << "\n";

    // --- Setup UDP sender ---
    UdpSender udp;
    if (!udp_target.empty()) {
        std::string udp_ip;
        int udp_port = 5000;
        if (!parseUdpArg(udp_target, udp_ip, udp_port)) {
            std::cerr << "[UDP] Invalid format. Expected IP:PORT (e.g. 127.0.0.1:5000)\n";
            return 1;
        }
        if (!udp.init(udp_ip, udp_port)) {
            std::cerr << "[UDP] Failed to create socket\n";
            return 1;
        }
    }

    // --- Setup JSONL output ---
    std::ofstream jsonl;
    if (!jsonl_path.empty()) {
        jsonl.open(jsonl_path, std::ios::out | std::ios::trunc);
        if (!jsonl.is_open()) {
            std::cerr << "[JSONL] Cannot open: " << jsonl_path << "\n";
            return 1;
        }
    }

    // --- Load data ---
    auto telemetry = loadTelemetry(csv_path);
    auto frames    = loadFrames(frames_dir);

    if (telemetry.empty()) {
        std::cerr << "[ERROR] No telemetry data.\n";
        return 1;
    }
    if (frames.empty()) {
        std::cerr << "[WARN] No frames found. Running in IMU-only mode.\n";
    }

    int W = 1920, H = 1080;
    if (!frames.empty()) {
        cv::Mat first = cv::imread(frames.front().path);
        if (!first.empty()) { W = first.cols; H = first.rows; }
        std::cout << "[Frames] Resolution: " << W << "x" << H << "\n";
    }

    DetectorConfig cfg;
    cfg.frame_width          = W;
    cfg.frame_height         = H;
    cfg.v_fov_degrees        = 90.0f;
    cfg.max_seconds_no_hailo = 0.5f;

    OptimizedHorizonDetector detector(cfg);
    detector.init("model.hef");

    std::cout << "\nStarting simulation (" << telemetry.size() << " steps)...\n";
    std::cout << "  Green  = vision-confirmed horizon\n";
    std::cout << "  Yellow = IMU-estimated horizon\n";
    if (show_window) std::cout << "  Any key = next | 'q' or ESC = quit\n";
    std::cout << "\n";

    int hailo_calls = 0, total_frames = 0;

    for (size_t i = 0; i < telemetry.size(); ++i) {
        const auto& tel = telemetry[i];

        ImuData imu;
        imu.roll  = static_cast<float>(tel.roll);
        imu.pitch = static_cast<float>(tel.pitch);
        detector.updateImu(imu);

        cv::Mat frame;
        if (!frames.empty()) {
            const FrameEntry* fe = findClosestFrame(frames, tel.timestamp_ms);
            if (fe) frame = cv::imread(fe->path);
        }

        // Provide a blank frame in IMU-only mode so process() still works
        if (frame.empty()) {
            frame = cv::Mat::zeros(H, W, CV_8UC3);
        }

        HorizonLine result = detector.process(frame);
        total_frames++;
        if (!result.is_estimated) hailo_calls++;

        bool detected = !(result.confidence == 0.0f && result.is_estimated
                          && result.angle == 0.0f && result.offset == 0.0f);

        auto pts = horizonToPoints(result, W, H);
        std::string json = toJson(tel, result, pts, detected);

        // Write JSONL
        if (jsonl.is_open()) {
            jsonl << json << "\n";
            jsonl.flush();
        }

        // Send UDP
        if (!udp_target.empty()) {
            udp.send(json);
        }

        std::cout << "t=" << tel.timestamp_ms
                  << " | pitch=" << std::fixed << std::setprecision(1) << tel.pitch
                  << " | roll="  << tel.roll
                  << " | offset=" << result.offset
                  << " | angle="  << result.angle
                  << " | conf="   << std::setprecision(2) << result.confidence
                  << (result.is_estimated ? " [IMU]" : " [VISION]")
                  << "\n";

        if (show_window && !frame.empty()) {
            drawHorizon(frame, result);
            cv::Mat display;
            float scale = 960.0f / W;
            cv::resize(frame, display, cv::Size(), scale, scale, cv::INTER_AREA);
            cv::imshow("AegisEdge - Horizon Detector", display);
            int key = cv::waitKey(1);
            if (key == 'q' || key == 27) break;
        }

        // Throttle to ~30 fps when streaming UDP
        if (!udp_target.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    }

    std::cout << "\n========== SIMULATION COMPLETE ==========\n";
    std::cout << "Frames processed : " << total_frames << "\n";
    std::cout << "Vision calls     : " << hailo_calls << "\n";
    std::cout << "IMU fallback     : " << (total_frames - hailo_calls) << "\n";
    if (total_frames > 0) {
        float pct = 100.f * (total_frames - hailo_calls) / total_frames;
        std::cout << "Compute savings  : " << std::fixed
                  << std::setprecision(1) << pct << "%\n";
    }

    if (show_window) cv::destroyAllWindows();
    return 0;
}
