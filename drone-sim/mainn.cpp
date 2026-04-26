#include "../DetectionSystems/horizon.hpp"
#include <opencv2/opencv.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <climits>
#include <cmath>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Estrutura de uma linha de telemetria do AirSim
// ---------------------------------------------------------------------------
struct TelemetryEntry {
    long long timestamp_ms;   // milissegundos desde epoch
    double    roll;           // graus
    double    pitch;          // graus
};

// ---------------------------------------------------------------------------
// Carrega o CSV de telemetria do AirSim
// Formato: timestamp_ms,drone_id,x,y,roll,pitch,yaw,altitude
// ---------------------------------------------------------------------------
std::vector<TelemetryEntry> loadTelemetry(const std::string& path) {
    std::vector<TelemetryEntry> entries;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[CSV] Nao foi possivel abrir: " << path << "\n";
        return entries;
    }

    std::string line;
    std::getline(file, line); // ignorar header

    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string token;
        TelemetryEntry e;

        std::getline(ss, token, ','); e.timestamp_ms = std::stoll(token);
        std::getline(ss, token, ','); // drone_id
        std::getline(ss, token, ','); // x
        std::getline(ss, token, ','); // y
        std::getline(ss, token, ','); e.roll  = std::stod(token);
        std::getline(ss, token, ','); e.pitch = std::stod(token);

        entries.push_back(e);
    }
    std::cout << "[CSV] " << entries.size() << " entradas de telemetria carregadas.\n";
    return entries;
}

// ---------------------------------------------------------------------------
// Frame com timestamp em nanosegundos (extraido do nome do ficheiro)
// ---------------------------------------------------------------------------
struct FrameEntry {
    long long   timestamp_ns;
    std::string path;
};

std::vector<FrameEntry> loadFrames(const std::string& dir) {
    std::vector<FrameEntry> frames;
    if (!fs::exists(dir)) {
        std::cerr << "[Frames] Diretorio nao existe: " << dir << "\n";
        return frames;
    }

    for (const auto& entry : fs::directory_iterator(dir)) {
        std::string name = entry.path().filename().string();
        // Formato: img_SimpleFlight__0_<timestamp_ns>.png
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
    std::cout << "[Frames] " << frames.size() << " frames encontradas.\n";
    return frames;
}

// ---------------------------------------------------------------------------
// Encontra a frame mais proxima de um timestamp (em ms)
// ---------------------------------------------------------------------------
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
// Desenha a linha do horizonte estimada sobre a frame
// ---------------------------------------------------------------------------
void drawHorizon(cv::Mat& frame, const HorizonLine& h) {
    const int W = frame.cols;
    const int H = frame.rows;

    // Centro do horizonte em coordenadas absolutas
    float center_y = H / 2.0f + h.offset;
    float angle_rad = h.angle * static_cast<float>(CV_PI) / 180.0f;

    float dx = (W / 2.0f) * std::cos(angle_rad);
    float dy = (W / 2.0f) * std::sin(angle_rad);

    cv::Point p1(static_cast<int>(W / 2.0f - dx),
                 static_cast<int>(center_y - dy));
    cv::Point p2(static_cast<int>(W / 2.0f + dx),
                 static_cast<int>(center_y + dy));

    // Verde = visao confirmada, Amarelo/Laranja = estimado por IMU
    cv::Scalar color = h.is_estimated ? cv::Scalar(0, 200, 255)
                                      : cv::Scalar(0, 255, 0);
    cv::line(frame, p1, p2, color, 2, cv::LINE_AA);

    // HUD
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1)
        << "angle: " << h.angle << " deg  "
        << "offset: " << h.offset << " px  "
        << (h.is_estimated ? "[IMU]" : "[VISAO]");
    cv::putText(frame, oss.str(), {10, 30},
                cv::FONT_HERSHEY_SIMPLEX, 0.7, color, 2);
}

// ---------------------------------------------------------------------------
// main()
// Uso:
//   ./sim                    -> usa paths por defeito
//   ./sim <csv> <frames_dir> -> paths personalizados
//   ./sim <csv> <dir> 0      -> sem janela (apenas terminal)
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {

    std::string csv_path    = "../Assets/logs/airsim_telemetry_log.csv";
    std::string frames_dir  = "../Assets/images/images";
    bool        show_window = true;

    if (argc > 1) csv_path   = argv[1];
    if (argc > 2) frames_dir = argv[2];
    if (argc > 3) show_window = std::string(argv[3]) != "0";

    auto telemetry = loadTelemetry(csv_path);
    auto frames    = loadFrames(frames_dir);

    if (telemetry.empty() || frames.empty()) {
        std::cerr << "[ERRO] Sem dados para simular.\n";
        return 1;
    }

    // Detectar resolucao real das frames
    cv::Mat first = cv::imread(frames.front().path);
    int W = first.cols, H = first.rows;
    std::cout << "[Frames] Resolucao detectada: " << W << "x" << H << "\n";

    DetectorConfig cfg;
    cfg.frame_width        = W;
    cfg.frame_height       = H;
    cfg.v_fov_degrees      = 90.0f;
    cfg.max_seconds_no_hailo = 0.5f;

    OptimizedHorizonDetector detector(cfg);
    detector.init("model.hef"); // sem hardware: cai em IMU-only (esperado)

    std::cout << "\nA iniciar simulacao (" << telemetry.size() << " passos)...\n";
    std::cout << "  Verde  = horizonte confirmado pela visao\n";
    std::cout << "  Amarelo = estimado apenas por IMU\n";
    if (show_window) {
        std::cout << "  Tecla qualquer = avancar | 'q' ou ESC = sair\n";
    }
    std::cout << "\n";

    int hailo_calls  = 0;
    int total_frames = 0;

    for (size_t i = 0; i < telemetry.size(); ++i) {
        const auto& tel = telemetry[i];

        const FrameEntry* fe = findClosestFrame(frames, tel.timestamp_ms);
        if (!fe) continue;

        cv::Mat frame = cv::imread(fe->path);
        if (frame.empty()) continue;

        ImuData imu;
        imu.roll  = static_cast<float>(tel.roll);
        imu.pitch = static_cast<float>(tel.pitch);
        detector.updateImu(imu);

        HorizonLine result = detector.process(frame);
        total_frames++;
        if (!result.is_estimated) hailo_calls++;

        drawHorizon(frame, result);

        std::cout << "t=" << tel.timestamp_ms
                  << " | pitch=" << std::fixed << std::setprecision(1) << tel.pitch
                  << " | roll="  << tel.roll
                  << " | offset=" << result.offset
                  << " | angle="  << result.angle
                  << (result.is_estimated ? " [IMU]" : " [VISAO]")
                  << "\n";

        if (show_window) {
            cv::Mat display;
            float scale = 960.0f / W;
            cv::resize(frame, display, cv::Size(),
                       scale, scale, cv::INTER_AREA);
            cv::imshow("AegisEdge - Horizon Detector", display);
            int key = cv::waitKey(0);
            if (key == 'q' || key == 27) break;
        }
    }

    std::cout << "\n========== SIMULACAO CONCLUIDA ==========\n";
    std::cout << "Frames processadas: " << total_frames << "\n";
    std::cout << "Chamadas a visao  : " << hailo_calls << "\n";
    std::cout << "Frames por IMU    : " << (total_frames - hailo_calls) << "\n";
    if (total_frames > 0) {
        float pct = 100.f * (total_frames - hailo_calls) / total_frames;
        std::cout << "Poupanca compute  : " << std::fixed
                  << std::setprecision(1) << pct << "%\n";
    }

    if (show_window) cv::destroyAllWindows();
    return 0;
}
