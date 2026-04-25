#include "horizon.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

// ===========================================================================
// Construtor / Destrutor
// ===========================================================================
OptimizedHorizonDetector::OptimizedHorizonDetector(const DetectorConfig& cfg)
    : cfg_(cfg)
    , pixels_per_degree_(cfg.frame_height / cfg.v_fov_degrees)
{}

OptimizedHorizonDetector::~OptimizedHorizonDetector() = default;

// ===========================================================================
// init() — carrega o modelo .hef e prepara o dispositivo Hailo
// Deve ser chamado uma vez antes de process().
// ===========================================================================
bool OptimizedHorizonDetector::init(const std::string& hef_path) {
#ifdef WITH_HAILO
    // --- 1. Criar o dispositivo virtual (detecta automaticamente o Hailo-8L) ---
    auto vdevice_exp = hailort::VDevice::create();
    if (!vdevice_exp) {
        std::cerr << "[Hailo] Falha ao criar VDevice: "
                  << vdevice_exp.status() << "\n";
        hailo_available_ = false;
        return false;
    }
    vdevice_ = vdevice_exp.release();

    // --- 2. Carregar o ficheiro .hef ---
    auto infer_model_exp = vdevice_->create_infer_model(hef_path);
    if (!infer_model_exp) {
        std::cerr << "[Hailo] Falha ao carregar HEF '" << hef_path << "': "
                  << infer_model_exp.status() << "\n";
        hailo_available_ = false;
        return false;
    }
    infer_model_ = infer_model_exp.release();
    infer_model_->set_batch_size(1);

    // --- 3. Configurar o formato de input ---
    // O modelo espera float32 em formato NHWC (batch, height, width, channels)
    auto& input = infer_model_->input();
    input->set_format_type(HAILO_FORMAT_TYPE_FLOAT32);

    // --- 4. Compilar e configurar a rede para inferência ---
    auto configured_exp = infer_model_->configure();
    if (!configured_exp) {
        std::cerr << "[Hailo] Falha ao configurar o modelo: "
                  << configured_exp.status() << "\n";
        hailo_available_ = false;
        return false;
    }
    configured_model_ = configured_exp.release();

    // --- 5. Pré-alocar buffers (evita malloc em tempo-real por frame) ---
    // Input: 3 canais (RGB) × H × W × float32
    const size_t input_size = 3 * cfg_.model_input_h * cfg_.model_input_w;
    input_buffer_.resize(input_size, 0.0f);

    // Output: 3 valores [sin(angle), cos(angle), offset_normalizado]
    output_buffer_.resize(3, 0.0f);

    std::cout << "[Hailo] Inicializado com sucesso. HEF: " << hef_path << "\n";
    hailo_available_ = true;
    return true;

#else
    (void)hef_path;
    std::cout << "[Hailo] Compilado sem suporte Hailo (-DWITH_HAILO não definido). "
              << "A correr em modo IMU-only.\n";
    hailo_available_ = false;
    return false;
#endif
}

// ===========================================================================
// updateImu() — thread-safe, chamado pelo driver do sensor
// ===========================================================================
void OptimizedHorizonDetector::updateImu(const ImuData& imu) {
    std::lock_guard<std::mutex> lock(imu_mutex_);
    current_imu_ = imu;
}

// ===========================================================================
// preprocessFrame() — BGR (OpenCV) → RGB → float32 normalizado no input_buffer_
// ===========================================================================
void OptimizedHorizonDetector::preprocessFrame(const cv::Mat& src) {
    // 1. Redimensionar para o tamanho de entrada do modelo
    cv::Mat resized;
    cv::resize(src, resized,
               cv::Size(cfg_.model_input_w, cfg_.model_input_h),
               0, 0, cv::INTER_LINEAR);

    // 2. BGR → RGB (OpenCV usa BGR por defeito, a maioria dos modelos espera RGB)
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

    // 3. Converter para float32 e normalizar: (pixel - mean) / std
    //    Resultado em formato planar: [R plane][G plane][B plane]
    //    (formato que a maioria dos modelos PyTorch exportados espera)
    int idx = 0;
    const int plane_size = cfg_.model_input_h * cfg_.model_input_w;

    for (int c = 0; c < 3; ++c) {
        for (int row = 0; row < rgb.rows; ++row) {
            const uchar* row_ptr = rgb.ptr<uchar>(row);
            for (int col = 0; col < rgb.cols; ++col) {
                float pixel = static_cast<float>(row_ptr[col * 3 + c]);
                input_buffer_[c * plane_size + row * rgb.cols + col] =
                    (pixel - cfg_.input_norm_mean) / cfg_.input_norm_std;
            }
        }
    }
    (void)idx;
}

// ===========================================================================
// callHailoInference() — pré-processa, corre a NPU, pós-processa
// Devolve offset relativo ao centro do crop (em pixels)
// ===========================================================================
HorizonLine OptimizedHorizonDetector::callHailoInference(const cv::Mat& cropped_frame) {
#ifdef WITH_HAILO
    if (!hailo_available_ || !configured_model_) {
        // Fallback silencioso — o process() vai usar o Kalman/IMU
        return {0.0f, 0.0f, true};
    }

    // --- Pré-processamento ---
    preprocessFrame(cropped_frame);

    // --- Criar bindings e apontar para os nossos buffers pré-alocados ---
    auto bindings_exp = configured_model_->create_bindings();
    if (!bindings_exp) {
        std::cerr << "[Hailo] Falha ao criar bindings\n";
        return {0.0f, 0.0f, true};
    }
    auto bindings = bindings_exp.release();

    // Apontar o buffer de input
    auto status = bindings.input()->set_buffer(
        hailort::MemoryView(input_buffer_.data(),
                            input_buffer_.size() * sizeof(float)));
    if (status != HAILO_SUCCESS) {
        std::cerr << "[Hailo] Falha ao definir buffer de input: " << status << "\n";
        return {0.0f, 0.0f, true};
    }

    // Apontar o buffer de output
    status = bindings.output()->set_buffer(
        hailort::MemoryView(output_buffer_.data(),
                            output_buffer_.size() * sizeof(float)));
    if (status != HAILO_SUCCESS) {
        std::cerr << "[Hailo] Falha ao definir buffer de output: " << status << "\n";
        return {0.0f, 0.0f, true};
    }

    // --- Executar inferência (timeout: 100ms — mais que suficiente para 5ms reais) ---
    status = configured_model_->run(bindings, std::chrono::milliseconds(100));
    if (status != HAILO_SUCCESS) {
        std::cerr << "[Hailo] Falha na inferência: " << status << "\n";
        return {0.0f, 0.0f, true};
    }

    // --- Pós-processamento ---
    // O modelo devolve: [sin(angle), cos(angle), offset_normalizado]
    //   sin/cos → evita a descontinuidade em ±180°
    //   offset_norm ∈ [-1.0, +1.0] → -1=topo do crop, 0=centro, +1=base
    float sin_angle    = output_buffer_[0];
    float cos_angle    = output_buffer_[1];
    float offset_norm  = output_buffer_[2];

    // Converter sin/cos → graus
    float angle_deg  = std::atan2f(sin_angle, cos_angle) * (180.0f / M_PI);

    // Converter offset normalizado → pixels relativos ao centro do crop
    float offset_px  = offset_norm * (cropped_frame.rows / 2.0f);

    return {angle_deg, offset_px, false};

#else
    // Sem Hailo: devolve horizonte centrado (simulação / desenvolvimento)
    (void)cropped_frame;
    return {0.0f, 0.0f, false};
#endif
}

// ===========================================================================
// adaptiveRoiHeight() — ROI cresce com a incerteza do Kalman
// ===========================================================================
int OptimizedHorizonDetector::adaptiveRoiHeight() const {
    float uncertainty = kalman_.uncertainty_px();
    int roi = cfg_.roi_height_base
            + static_cast<int>(uncertainty * cfg_.roi_uncertainty_scale);
    return std::min(roi, cfg_.frame_height);
}

// ===========================================================================
// process() — pipeline principal
// ===========================================================================
HorizonLine OptimizedHorizonDetector::process(const cv::Mat& frame) {

    // --- 0. Snapshot thread-safe do IMU ---
    ImuData imu, prev_imu;
    {
        std::lock_guard<std::mutex> lock(imu_mutex_);
        imu      = current_imu_;
        prev_imu = last_imu_;
    }

    // --- 1. Delta de tempo real entre frames ---
    auto  now = Clock::now();
    float dt  = std::chrono::duration<float>(now - last_process_time_).count();
    dt = std::clamp(dt, 1e-4f, 0.5f);
    last_process_time_ = now;

    float seconds_since_hailo =
        std::chrono::duration<float>(now - last_hailo_time_).count();

    // --- 2. Taxa angular do IMU → pixels/s (feedforward do Kalman) ---
    float pitch_rate_px_s = ((imu.pitch - prev_imu.pitch) / dt) * pixels_per_degree_;

    // --- 3. Propagação do Kalman (sempre) ---
    kalman_.predict(dt, pitch_rate_px_s);

    // --- 4. Decisão: chamar o Hailo? ---
    float delta_pitch = std::abs(imu.pitch - prev_imu.pitch);
    float delta_roll  = std::abs(imu.roll  - prev_imu.roll );

    bool needs_hailo = !initialized_
                    || (delta_pitch > cfg_.imu_threshold_degrees)
                    || (delta_roll  > cfg_.imu_threshold_degrees)
                    || (seconds_since_hailo >= cfg_.max_seconds_no_hailo);

    if (needs_hailo && hailo_available_) {
        // --- CAMINHO PESADO: Hailo com ROI dinâmico ---

        // Centro previsto pelo Kalman (mais fiável que só o pitch do IMU)
        float predicted_center_y = (cfg_.frame_height / 2.0f) + kalman_.offset_y;

        int roi_height = adaptiveRoiHeight();
        int y_start    = static_cast<int>(predicted_center_y) - (roi_height / 2);
        y_start        = std::clamp(y_start, 0, cfg_.frame_height - roi_height);

        cv::Mat cropped_frame = frame(cv::Rect(0, y_start, cfg_.frame_width, roi_height));

        HorizonLine hailo_result = callHailoInference(cropped_frame);

        if (!hailo_result.is_estimated) {
            // Translação de coordenadas: centro do crop → centro da imagem
            int   crop_center_y   = y_start + (roi_height / 2);
            int   image_center_y  = cfg_.frame_height / 2;
            float absolute_offset = (crop_center_y - image_center_y)
                                  + hailo_result.offset;

            kalman_.update(absolute_offset, kalman_.R_hailo);
            current_angle_   = hailo_result.angle;
            last_hailo_time_ = now;
            initialized_     = true;
        }

    } else {
        // --- CAMINHO RÁPIDO: Dead Reckoning (Kalman já propagado) ---
        // Correcção suave com estimativa do IMU para travar a deriva
        float imu_predicted_offset = imu.pitch * pixels_per_degree_;
        kalman_.update(imu_predicted_offset, kalman_.R_imu);

        float roll_diff  = imu.roll - prev_imu.roll;
        current_angle_  += roll_diff;

        // Se não temos Hailo disponível, forçamos initialized a true
        // depois da primeira frame para não ficar preso no caminho pesado
        if (!hailo_available_) initialized_ = true;
    }

    // --- 5. Guardar estado IMU ---
    {
        std::lock_guard<std::mutex> lock(imu_mutex_);
        last_imu_ = imu;
    }

    // --- 6. Resultado final ---
    return {
        current_angle_,
        kalman_.offset_y,
        !initialized_ || (seconds_since_hailo > cfg_.max_seconds_no_hailo)
    };
}
