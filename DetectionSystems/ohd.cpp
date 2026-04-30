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
const char* OptimizedHorizonDetector::visionModeName() {
#if defined(WITH_HAILO)
    return "IMU + Hailo (NPU)";
#elif defined(WITH_HOUGH)
    return "IMU + Hough (CV classico)";
#else
    return "IMU only (sem visao)";
#endif
}

// ===========================================================================
bool OptimizedHorizonDetector::init(const std::string& hef_path) {
    std::cout << "[Detector] Modo: " << visionModeName() << "\n";

#if defined(WITH_HAILO)
    // ---- 1. Criar VDevice ----
    auto vdevice_exp = hailort::VDevice::create();
    if (!vdevice_exp) {
        std::cerr << "[Hailo] Falha ao criar VDevice (status="
                  << vdevice_exp.status() << ")\n";
        vision_available_ = false;
        return false;
    }
    vdevice_ = vdevice_exp.release();

    // ---- 2. Criar InferModel a partir do .hef ----
    auto infer_model_exp = vdevice_->create_infer_model(hef_path);
    if (!infer_model_exp) {
        std::cerr << "[Hailo] Falha ao carregar HEF '" << hef_path
                  << "' (status=" << infer_model_exp.status() << ")\n";
        vision_available_ = false;
        return false;
    }
    infer_model_ = infer_model_exp.release();
    infer_model_->set_batch_size(1);

    // ---- 3. Descobrir nomes de input e output ----
    auto input_names  = infer_model_->get_input_names();
    auto output_names = infer_model_->get_output_names();
    if (input_names.empty() || output_names.empty()) {
        std::cerr << "[Hailo] Modelo sem inputs ou outputs.\n";
        vision_available_ = false;
        return false;
    }
    input_name_  = input_names[0];
    output_name_ = output_names[0];

    // ---- 4. Definir formato do input para float32 ----
    // input() devolve por valor — guardar em variavel local primeiro
    auto input_stream = infer_model_->input(input_name_);
    if (!input_stream) {
        std::cerr << "[Hailo] Falha ao obter input stream.\n";
        vision_available_ = false;
        return false;
    }
    input_stream->set_format_type(HAILO_FORMAT_TYPE_FLOAT32);

    auto output_stream = infer_model_->output(output_name_);
    if (!output_stream) {
        std::cerr << "[Hailo] Falha ao obter output stream.\n";
        vision_available_ = false;
        return false;
    }
    output_stream->set_format_type(HAILO_FORMAT_TYPE_FLOAT32);

    // ---- 5. Configurar o modelo ----
    auto configured_exp = infer_model_->configure();
    if (!configured_exp) {
        std::cerr << "[Hailo] Falha ao configurar modelo (status="
                  << configured_exp.status() << ")\n";
        vision_available_ = false;
        return false;
    }
    // Novo API: configure() devolve ConfiguredInferModel por valor.
    // Movemo-lo para o nosso unique_ptr.
    configured_model_ =
        std::make_unique<hailort::ConfiguredInferModel>(configured_exp.release());

    // ---- 6. Pre-alocar buffers ----
    const size_t input_size = 3 * cfg_.model_input_h * cfg_.model_input_w;
    input_buffer_.resize(input_size, 0.0f);
    output_buffer_.resize(3, 0.0f);

    std::cout << "[Hailo] Inicializado com sucesso.\n";
    std::cout << "        HEF:    " << hef_path << "\n";
    std::cout << "        Input:  " << input_name_  << "\n";
    std::cout << "        Output: " << output_name_ << "\n";

    vision_available_ = true;
    return true;

#elif defined(WITH_HOUGH)
    (void)hef_path;
    std::cout << "[Hough] Pronto a usar.\n";
    vision_available_ = true;
    return true;

#else
    (void)hef_path;
    std::cout << "[IMU] Sem visao - corre apenas com Kalman + IMU.\n";
    vision_available_ = false;
    return true;
#endif
}

// ===========================================================================
void OptimizedHorizonDetector::updateImu(const ImuData& imu) {
    std::lock_guard<std::mutex> lock(imu_mutex_);
    current_imu_ = imu;
}

// ===========================================================================
void OptimizedHorizonDetector::preprocessFrame(const cv::Mat& src) {
#if defined(WITH_HAILO)
    cv::Mat resized;
    cv::resize(src, resized,
               cv::Size(cfg_.model_input_w, cfg_.model_input_h),
               0, 0, cv::INTER_LINEAR);

    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

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
#else
    (void)src;
#endif
}

// ===========================================================================
float OptimizedHorizonDetector::computeSkyGroundContrast(const cv::Mat& gray, float mid_y, float angle_deg) {
    if (gray.empty()) return 0.0f;

    // Use two 20px wide bands above and below mid_y
    int band_h = 20;
    int margin = 5;
    
    int sky_y    = static_cast<int>(mid_y) - margin - band_h;
    int ground_y = static_cast<int>(mid_y) + margin;
    
    if (sky_y < 0 || ground_y + band_h > gray.rows) return 0.0f;

    cv::Rect sky_roi(gray.cols/4, sky_y, gray.cols/2, band_h);
    cv::Rect ground_roi(gray.cols/4, ground_y, gray.cols/2, band_h);

    float sky_mean = cv::mean(gray(sky_roi))[0];
    float ground_mean = cv::mean(gray(ground_roi))[0];

    return sky_mean - ground_mean;
}

// ===========================================================================
HorizonLine OptimizedHorizonDetector::callVision(const cv::Mat& cropped_frame, float expected_angle) {

#if defined(WITH_HAILO)
    if (!vision_available_ || !configured_model_) 
        return {0.0f, 0.0f, true, 0.0f, "IMU_ESTIMATED", expected_angle, 0.0f};
    
    preprocessFrame(cropped_frame);

    auto bindings_exp = configured_model_->create_bindings();
    if (!bindings_exp) return {0.0f, 0.0f, true, 0.0f, "IMU_ESTIMATED", expected_angle, 0.0f};
    auto bindings = bindings_exp.release();

    auto in_status = bindings.input(input_name_)->set_buffer(
        hailort::MemoryView(input_buffer_.data(),
                            input_buffer_.size() * sizeof(float)));
    if (in_status != HAILO_SUCCESS) return {0.0f, 0.0f, true, 0.0f, "IMU_ESTIMATED", expected_angle, 0.0f};

    auto out_status = bindings.output(output_name_)->set_buffer(
        hailort::MemoryView(output_buffer_.data(),
                            output_buffer_.size() * sizeof(float)));
    if (out_status != HAILO_SUCCESS) return {0.0f, 0.0f, true, 0.0f, "IMU_ESTIMATED", expected_angle, 0.0f};

    auto run_status = configured_model_->run(bindings,
                                             std::chrono::milliseconds(100));
    if (run_status != HAILO_SUCCESS) return {0.0f, 0.0f, true, 0.0f, "IMU_ESTIMATED", expected_angle, 0.0f};

    float sin_angle   = output_buffer_[0];
    float cos_angle   = output_buffer_[1];
    float offset_norm = output_buffer_[2];

    float angle_deg = std::atan2f(sin_angle, cos_angle)
                    * (180.0f / static_cast<float>(M_PI));
    float offset_px = offset_norm * (cropped_frame.rows / 2.0f);
    return {angle_deg, offset_px, false, 0.82f, "VISION_HAILO", expected_angle, 0.0f};

#elif defined(WITH_HOUGH)
    if (cropped_frame.empty()) return {0.0f, 0.0f, true, 0.0f, "IMU_ESTIMATED", expected_angle, 0.0f};

    cv::Mat gray;
    cv::cvtColor(cropped_frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(3, 3), 1.0);

    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    int min_length = std::max(30, cropped_frame.cols / 5);
    int threshold  = std::max(20, cropped_frame.cols / 8);
    int max_gap    = std::max(5,  cropped_frame.cols / 16);

    std::vector<cv::Vec4i> lines;
    cv::HoughLinesP(edges, lines, 1, CV_PI / 180.0,
                    threshold, min_length, max_gap);

    if (lines.empty()) return {0.0f, 0.0f, true, 0.0f, "IMU_ESTIMATED", expected_angle, 0.0f};

    // Priors for gating
    float prior_y_in_crop = cropped_frame.rows * 0.5f;
    
    float sigma_y     = cropped_frame.rows * 0.25f;
    float sigma_angle = 6.0f;

    float best_score   = 0.0f;
    float best_angle   = 0.0f;
    float best_y       = 0.0f;
    int   best_length  = 0;

    for (const auto& l : lines) {
        float dx = static_cast<float>(l[2] - l[0]);
        float dy = static_cast<float>(l[3] - l[1]);
        if (dx < 0) { dx = -dx; dy = -dy; }
        if (dx < 1e-3f) continue;

        float angle_deg = std::atan2(dy, dx) * 180.0f / static_cast<float>(CV_PI);
        
        // Gating 1: Angle Error
        float angle_error = std::abs(angle_deg - expected_angle);
        if (angle_error > cfg_.max_hough_angle_error_deg) continue;

        float length = std::sqrt(dx * dx + dy * dy);
        float mid_y  = (l[1] + l[3]) * 0.5f;

        // Gating 2: Vertical Position Error
        float dist_y = std::abs(mid_y - prior_y_in_crop);
        if (dist_y > cropped_frame.rows * cfg_.max_hough_y_error_ratio) continue;

        // Scoring: length * Gaussian(dist_y) * Gaussian(angle_error)
        float score = length 
                    * std::exp(-(dist_y * dist_y) / (2 * sigma_y * sigma_y))
                    * std::exp(-(angle_error * angle_error) / (2 * sigma_angle * sigma_angle));

        if (score > best_score) {
            best_score  = score;
            best_angle  = angle_deg;
            best_y      = mid_y;
            best_length = static_cast<int>(length);
        }
    }

    if (best_score < 10.0f || best_length < min_length) {
        return {0.0f, 0.0f, true, 0.0f, "IMU_ESTIMATED", expected_angle, 0.0f};
    }

    // Sky/Ground Contrast Check
    float contrast = computeSkyGroundContrast(gray, best_y, best_angle);
    float contrast_penalty = 1.0f;
    if (contrast < 0) contrast_penalty = 0.2f; // Ground brighter than sky? High suspicion
    else if (contrast < cfg_.min_sky_ground_contrast) contrast_penalty = 0.6f;

    float offset_from_crop_center = best_y - cropped_frame.rows * 0.5f;
    float conf = std::min(0.95f, (0.40f + 0.002f * best_score) * contrast_penalty);
    float angle_err = std::abs(best_angle - expected_angle);
    
    return {best_angle, offset_from_crop_center, false, conf, "VISION_HOUGH", expected_angle, angle_err};

#else
    (void)cropped_frame;
    (void)expected_angle;
    return {0.0f, 0.0f, true, 0.0f, "IMU_ESTIMATED", 0.0f, 0.0f};
#endif
}

// ===========================================================================
int OptimizedHorizonDetector::adaptiveRoiHeight() const {
    float uncertainty = kalman_.uncertainty_px();
    int roi = cfg_.roi_height_base
            + static_cast<int>(uncertainty * cfg_.roi_uncertainty_scale);
    return std::min(roi, cfg_.frame_height);
}

// ===========================================================================
HorizonLine OptimizedHorizonDetector::process(const cv::Mat& frame) {

    ImuData imu, prev_imu;
    {
        std::lock_guard<std::mutex> lock(imu_mutex_);
        imu      = current_imu_;
        prev_imu = last_imu_;
    }

    auto  now = Clock::now();
    float dt  = std::chrono::duration<float>(now - last_process_time_).count();
    dt = std::clamp(dt, 1e-4f, 0.5f);
    last_process_time_ = now;

    float seconds_since_hailo =
        std::chrono::duration<float>(now - last_hailo_time_).count();

    float pitch_rate_px_s = ((imu.pitch - prev_imu.pitch) / dt) * pixels_per_degree_;
    kalman_.predict(dt, pitch_rate_px_s);

    float delta_pitch = std::abs(imu.pitch - prev_imu.pitch);
    float delta_roll  = std::abs(imu.roll  - prev_imu.roll);

    bool needs_vision = !initialized_
                     || (delta_pitch > cfg_.imu_threshold_degrees)
                     || (delta_roll  > cfg_.imu_threshold_degrees)
                     || (seconds_since_hailo >= cfg_.max_seconds_no_hailo);

    bool got_vision = false;
    std::string current_source = "IMU_ESTIMATED";
    float final_confidence = 0.0f;
    float expected_angle = cfg_.imu_roll_to_horizon_angle_sign * imu.roll;
    float final_angle_error = 0.0f;

    static int log_throttle = 0;
    log_throttle++;

#if HAS_VISION
    if (needs_vision && vision_available_) {
        float predicted_center_y = (cfg_.frame_height / 2.0f) + kalman_.offset_y;
        int   roi_height = adaptiveRoiHeight();
        int   y_start    = static_cast<int>(predicted_center_y) - (roi_height / 2);
        y_start = std::clamp(y_start, 0, cfg_.frame_height - roi_height);

        cv::Mat cropped_frame =
            frame(cv::Rect(0, y_start, cfg_.frame_width, roi_height));

        HorizonLine vision_result = callVision(cropped_frame, expected_angle);

        if (!vision_result.is_estimated) {
            int   crop_center_y   = y_start + (roi_height / 2);
            int   image_center_y  = cfg_.frame_height / 2;
            float absolute_offset = (crop_center_y - image_center_y)
                                  + vision_result.offset;

            float correction = std::abs(absolute_offset - kalman_.offset_y);
            float gate_threshold = 2.0f * kalman_.uncertainty_px() + 20.0f;

            bool reject = false;
            std::string reason = "";

            if (correction > cfg_.max_hough_offset_correction_px && initialized_) {
                reject = true; reason = "correction too large (" + std::to_string(correction) + ")";
            } else if (vision_result.confidence < cfg_.min_hough_confidence) {
                reject = true; reason = "low confidence (" + std::to_string(vision_result.confidence) + ")";
            } else if (initialized_ && correction > gate_threshold) {
                reject = true; reason = "gating rejection";
            }

            if (!reject) {
                kalman_.update(absolute_offset, kalman_.R_hailo);
                current_angle_   = vision_result.angle;
                last_hailo_time_ = now;
                initialized_     = true;
                got_vision       = true;
                final_confidence = vision_result.confidence;
                current_source   = vision_result.source;
                final_angle_error = vision_result.angle_error;
            } else {
                if (log_throttle % 60 == 0) {
                    std::cout << "[HORIZON] Reject Hough: " << reason << std::endl;
                }
            }
        }
    }
#else
    (void)frame;
    (void)needs_vision;
    (void)seconds_since_hailo;
#endif

    if (!got_vision) {
        float roll_diff = imu.roll - prev_imu.roll;
        current_angle_ -= roll_diff;

        float imu_predicted_offset = imu.pitch * pixels_per_degree_;
        kalman_.update(imu_predicted_offset, kalman_.R_imu);

        final_confidence = std::max(
            0.30f,
            0.55f - 0.005f * kalman_.uncertainty_px()
        );
        current_source = "IMU_ESTIMATED";
        final_angle_error = 0.0f;

#if !HAS_VISION
        initialized_ = true;
#endif
    }

    {
        std::lock_guard<std::mutex> lock(imu_mutex_);
        last_imu_ = imu;
    }

    return { current_angle_, kalman_.offset_y, !got_vision, final_confidence, current_source, expected_angle, final_angle_error };
}
