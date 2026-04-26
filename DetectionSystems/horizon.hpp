#pragma once
#include <opencv2/opencv.hpp>
#include <mutex>
#include <chrono>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Modos de compilacao (mutuamente exclusivos para a fonte de visao):
//
//   (sem flag)        -> IMU only      (sem visao, so Kalman+IMU)
//   -DWITH_HOUGH      -> IMU + Hough   (CV classico como proxy)
//   -DWITH_HAILO      -> IMU + Hailo   (NPU real)
//
// Se ambos os flags forem definidos, WITH_HAILO tem prioridade.
// ---------------------------------------------------------------------------

#ifdef WITH_HAILO
#include <hailo/hailort.hpp>
// Se WITH_HAILO estiver definido, desactivamos o Hough mesmo que o utilizador
// passe ambos os flags por engano.
#ifdef WITH_HOUGH
#undef WITH_HOUGH
#endif
#endif

// Flag interna: ha alguma fonte de visao disponivel?
#if defined(WITH_HAILO) || defined(WITH_HOUGH)
#define HAS_VISION 1
#else
#define HAS_VISION 0
#endif

// ---------------------------------------------------------------------------
// Estruturas de dados
// ---------------------------------------------------------------------------

struct ImuData {
    float roll;
    float pitch;
};

struct HorizonLine {
    float angle;
    float offset;
    bool  is_estimated;
    float confidence;  // 0.7-0.9 vision, 0.35-0.55 IMU, 0.0 failed
};

// ---------------------------------------------------------------------------
// Configuracao
// ---------------------------------------------------------------------------
struct DetectorConfig {
    int   frame_width           = 1920;
    int   frame_height          = 1080;
    float v_fov_degrees         = 90.0f;

    float imu_threshold_degrees = 1.5f;
    int   roi_height_base       = 256;
    float max_seconds_no_hailo  = 0.5f;
    int   roi_uncertainty_scale = 4;

    int   model_input_w         = 224;
    int   model_input_h         = 224;
    float input_norm_mean       = 0.0f;
    float input_norm_std        = 255.0f;
};

// ---------------------------------------------------------------------------
// Filtro de Kalman 1-D
// ---------------------------------------------------------------------------
struct KalmanHorizon {
    float offset_y   = 0.0f;
    float velocity_y = 0.0f;

    float P_pos = 100.0f;
    float P_vel = 50.0f;

    const float Q_pos   = 0.5f;
    const float Q_vel   = 5.0f;
    const float R_imu   = 2.0f;
    const float R_hailo = 1.0f;

    void predict(float dt, float imu_pitch_rate_px_per_s) {
        offset_y   += velocity_y * dt;
        velocity_y += imu_pitch_rate_px_per_s * dt;
        P_pos += P_vel * dt * dt + Q_pos * dt;
        P_vel += Q_vel * dt;
    }

    void update(float measured_offset, float R) {
        float K  = P_pos / (P_pos + R);
        offset_y += K * (measured_offset - offset_y);
        P_pos     = (1.0f - K) * P_pos;
    }

    float uncertainty_px() const { return std::sqrt(P_pos); }
};

// ---------------------------------------------------------------------------
// Detector
// ---------------------------------------------------------------------------
class OptimizedHorizonDetector {
public:
    explicit OptimizedHorizonDetector(const DetectorConfig& cfg = DetectorConfig());
    ~OptimizedHorizonDetector();

    // Indica qual o modo activo (string para imprimir no arranque)
    static const char* visionModeName();

    // Carrega o .hef. Apenas relevante em modo Hailo.
    // Em modo Hough ou IMU-only, retorna true sem fazer nada.
    bool init(const std::string& hef_path = "");

    void updateImu(const ImuData& imu);
    HorizonLine process(const cv::Mat& frame);

private:
    DetectorConfig cfg_;
    float          pixels_per_degree_;

    mutable std::mutex imu_mutex_;
    ImuData last_imu_    = {0.0f, 0.0f};
    ImuData current_imu_ = {0.0f, 0.0f};

    KalmanHorizon kalman_;
    float current_angle_ = 0.0f;

    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    TimePoint last_hailo_time_   = Clock::now() - std::chrono::seconds(10);
    TimePoint last_process_time_ = Clock::now();

    bool initialized_     = false;
    bool vision_available_ = false;

    std::vector<float> input_buffer_;
    std::vector<float> output_buffer_;

#ifdef WITH_HAILO
    std::unique_ptr<hailort::VDevice>              vdevice_;
    std::shared_ptr<hailort::InferModel>           infer_model_;
    std::shared_ptr<hailort::ConfiguredInferModel> configured_model_;
#endif

    HorizonLine callVision(const cv::Mat& cropped_frame);
    void        preprocessFrame(const cv::Mat& src);
    int         adaptiveRoiHeight() const;
};
