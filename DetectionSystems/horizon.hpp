#pragma once
#include <opencv2/opencv.hpp>
#include <mutex>
#include <chrono>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Guard de compilação: permite compilar sem o SDK do Hailo instalado.
// Sem Hailo: o sistema corre em modo IMU-only (útil para simulação).
// Com Hailo: compila com -DWITH_HAILO e linka com -lhailort
// ---------------------------------------------------------------------------
#ifdef WITH_HAILO
#include <hailo/hailort.hpp>
#endif

// ---------------------------------------------------------------------------
// Estruturas de dados
// ---------------------------------------------------------------------------

struct ImuData {
    float roll;   // graus, positivo = roll para a direita
    float pitch;  // graus, positivo = nariz para cima
};

struct HorizonLine {
    float angle;        // graus
    float offset;       // pixels relativos ao centro da imagem (Y=0 é o centro)
    bool  is_estimated; // true = apenas IMU | false = validado pela rede
};

// ---------------------------------------------------------------------------
// Filtro de Kalman 1-D
// Estado: [ offset_y (px), velocidade_y (px/s) ]
// ---------------------------------------------------------------------------
struct KalmanHorizon {
    float offset_y   = 0.0f;
    float velocity_y = 0.0f;

    float P_pos = 100.0f;   // incerteza inicial posição  (px²)
    float P_vel = 50.0f;    // incerteza inicial velocidade

    const float Q_pos   = 0.5f;   // ruído de processo – posição
    const float Q_vel   = 5.0f;   // ruído de processo – velocidade
    const float R_imu   = 2.0f;   // ruído de medição IMU   (px²)
    const float R_hailo = 1.0f;   // ruído de medição Hailo (px²)

    void predict(float dt, float imu_pitch_rate_px_per_s) {
        offset_y   += velocity_y * dt;
        velocity_y += imu_pitch_rate_px_per_s * dt; // feedforward IMU
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
// Detector principal
// ---------------------------------------------------------------------------
class OptimizedHorizonDetector {
public:
    struct Config {
        // Câmara
        int   frame_width           = 1920;
        int   frame_height          = 1080;
        float v_fov_degrees         = 90.0f;   // ← calibra com a tua câmara real

        // Lógica de decisão
        float imu_threshold_degrees = 1.5f;
        int   roi_height_base       = 256;
        float max_seconds_no_hailo  = 0.5f;
        int   roi_uncertainty_scale = 4;       // px de ROI extra por px de incerteza

        // Modelo de rede neural
        int   model_input_w    = 224;
        int   model_input_h    = 224;
        // Normalização: pixel_float = (pixel_uint8 - mean) / std
        // ImageNet standard (ajusta se o teu modelo foi treinado diferente)
        float input_norm_mean  = 0.0f;
        float input_norm_std   = 255.0f;
    };

    explicit OptimizedHorizonDetector(const Config& cfg = {});
    ~OptimizedHorizonDetector();

    // Carrega o .hef e inicializa o dispositivo Hailo.
    // Chama uma vez antes de process().
    // Devolve false se falhar — o sistema continua em modo IMU-only.
    bool init(const std::string& hef_path);

    // Thread-safe: pode ser chamado do driver do IMU a qualquer momento
    void updateImu(const ImuData& imu);

    // Pipeline principal — chamado na thread da câmara
    HorizonLine process(const cv::Mat& frame);

private:
    Config cfg_;
    float  pixels_per_degree_;

    // Estado do IMU (protegido por mutex — threads diferentes)
    mutable std::mutex imu_mutex_;
    ImuData last_imu_    = {0.0f, 0.0f};
    ImuData current_imu_ = {0.0f, 0.0f};

    // Kalman
    KalmanHorizon kalman_;
    float current_angle_ = 0.0f;

    // Temporização real (substitui contagem de frames)
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    TimePoint last_hailo_time_   = Clock::now() - std::chrono::seconds(10);
    TimePoint last_process_time_ = Clock::now();

    bool initialized_     = false;
    bool hailo_available_ = false;

    // Buffers de inferência — alocados uma vez em init() para evitar malloc por frame
    std::vector<float> input_buffer_;   // [C * H * W] float32 normalizado
    std::vector<float> output_buffer_;  // [3] → [sin(angle), cos(angle), offset_norm]

#ifdef WITH_HAILO
    std::unique_ptr<hailort::VDevice>              vdevice_;
    std::shared_ptr<hailort::InferModel>           infer_model_;
    std::shared_ptr<hailort::ConfiguredInferModel> configured_model_;
#endif

    // Métodos internos
    HorizonLine callHailoInference(const cv::Mat& cropped_frame);
    void        preprocessFrame(const cv::Mat& src);   // BGR → RGB → float normalizado
    int         adaptiveRoiHeight() const;
};
