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
// visionModeName() - string identificadora do modo activo
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
// init()
// ===========================================================================
bool OptimizedHorizonDetector::init(const std::string& hef_path) {
    std::cout << "[Detector] Modo: " << visionModeName() << "\n";

#if defined(WITH_HAILO)
    // ---- MODO HAILO ----
    auto vdevice_exp = hailort::VDevice::create();
    if (!vdevice_exp) {
        std::cerr << "[Hailo] Falha ao criar VDevice\n";
        vision_available_ = false;
        return false;
    }
    vdevice_ = vdevice_exp.release();

    auto infer_model_exp = vdevice_->create_infer_model(hef_path);
    if (!infer_model_exp) {
        std::cerr << "[Hailo] Falha ao carregar HEF '" << hef_path << "'\n";
        vision_available_ = false;
        return false;
    }
    infer_model_ = infer_model_exp.release();
    infer_model_->set_batch_size(1);

    auto& input = infer_model_->input();
    input->set_format_type(HAILO_FORMAT_TYPE_FLOAT32);

    auto configured_exp = infer_model_->configure();
    if (!configured_exp) {
        std::cerr << "[Hailo] Falha ao configurar o modelo\n";
        vision_available_ = false;
        return false;
    }
    configured_model_ = configured_exp.release();

    const size_t input_size = 3 * cfg_.model_input_h * cfg_.model_input_w;
    input_buffer_.resize(input_size, 0.0f);
    output_buffer_.resize(3, 0.0f);

    std::cout << "[Hailo] Inicializado. HEF: " << hef_path << "\n";
    vision_available_ = true;
    return true;

#elif defined(WITH_HOUGH)
    // ---- MODO HOUGH ----
    (void)hef_path;
    std::cout << "[Hough] Pronto a usar.\n";
    vision_available_ = true;
    return true;

#else
    // ---- MODO IMU-ONLY ----
    (void)hef_path;
    std::cout << "[IMU] Sem visao - corre apenas com Kalman + IMU.\n";
    vision_available_ = false;
    return true;
#endif
}

// ===========================================================================
// updateImu()
// ===========================================================================
void OptimizedHorizonDetector::updateImu(const ImuData& imu) {
    std::lock_guard<std::mutex> lock(imu_mutex_);
    current_imu_ = imu;
}

// ===========================================================================
// preprocessFrame() - so usado em modo Hailo
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
// callVision() - dispatch consoante o modo
// ===========================================================================
HorizonLine OptimizedHorizonDetector::callVision(const cv::Mat& cropped_frame) {

#if defined(WITH_HAILO)
    // ---- HAILO ----
    if (!vision_available_ || !configured_model_) return {0.0f, 0.0f, true};
    preprocessFrame(cropped_frame);

    auto bindings_exp = configured_model_->create_bindings();
    if (!bindings_exp) return {0.0f, 0.0f, true};
    auto bindings = bindings_exp.release();

    auto status = bindings.input()->set_buffer(
        hailort::MemoryView(input_buffer_.data(),
                            input_buffer_.size() * sizeof(float)));
    if (status != HAILO_SUCCESS) return {0.0f, 0.0f, true};

    status = bindings.output()->set_buffer(
        hailort::MemoryView(output_buffer_.data(),
                            output_buffer_.size() * sizeof(float)));
    if (status != HAILO_SUCCESS) return {0.0f, 0.0f, true};

    status = configured_model_->run(bindings, std::chrono::milliseconds(100));
    if (status != HAILO_SUCCESS) return {0.0f, 0.0f, true};

    float sin_angle   = output_buffer_[0];
    float cos_angle   = output_buffer_[1];
    float offset_norm = output_buffer_[2];

    float angle_deg = std::atan2f(sin_angle, cos_angle)
                    * (180.0f / static_cast<float>(M_PI));
    float offset_px = offset_norm * (cropped_frame.rows / 2.0f);
    return {angle_deg, offset_px, false};

#elif defined(WITH_HOUGH)
    // ---- HOUGH (escolhe a UNICA linha mais consistente com o prior) ----
    if (cropped_frame.empty()) return {0.0f, 0.0f, true};

    cv::Mat gray;
    cv::cvtColor(cropped_frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(3, 3), 1.0);

    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    int min_length = std::max(20, cropped_frame.cols / 6);
    int threshold  = std::max(20, cropped_frame.cols / 8);
    int max_gap    = std::max(5,  cropped_frame.cols / 16);

    std::vector<cv::Vec4i> lines;
    cv::HoughLinesP(edges, lines, 1, CV_PI / 180.0,
                    threshold, min_length, max_gap);

    if (lines.empty()) return {0.0f, 0.0f, true};

    // Prior: o ROI ja esta centrado em torno do prior do Kalman, logo
    // o horizonte esperado esta proximo do meio do crop.
    float prior_y_in_crop = cropped_frame.rows * 0.5f;
    float sigma           = cropped_frame.rows * 0.3f;

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
        if (std::abs(angle_deg) > 30.0f) continue;

        float length = std::sqrt(dx * dx + dy * dy);
        float mid_y  = (l[1] + l[3]) * 0.5f;

        float dist  = std::abs(mid_y - prior_y_in_crop);
        float score = length * std::exp(-dist / sigma);

        if (score > best_score) {
            best_score  = score;
            best_angle  = angle_deg;
            best_y      = mid_y;
            best_length = static_cast<int>(length);
        }
    }

    if (best_score < 1.0f || best_length < min_length) {
        return {0.0f, 0.0f, true};
    }

    float offset_from_crop_center = best_y - cropped_frame.rows * 0.5f;
    return {best_angle, offset_from_crop_center, false};

#else
    // ---- IMU-ONLY: nao deveriamos sequer ser chamados ----
    (void)cropped_frame;
    return {0.0f, 0.0f, true};
#endif
}

// ===========================================================================
// adaptiveRoiHeight()
// ===========================================================================
int OptimizedHorizonDetector::adaptiveRoiHeight() const {
    float uncertainty = kalman_.uncertainty_px();
    int roi = cfg_.roi_height_base
            + static_cast<int>(uncertainty * cfg_.roi_uncertainty_scale);
    return std::min(roi, cfg_.frame_height);
}

// ===========================================================================
// process()
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

#if HAS_VISION
    if (needs_vision && vision_available_) {
        float predicted_center_y = (cfg_.frame_height / 2.0f) + kalman_.offset_y;
        int   roi_height = adaptiveRoiHeight();
        int   y_start    = static_cast<int>(predicted_center_y) - (roi_height / 2);
        y_start = std::clamp(y_start, 0, cfg_.frame_height - roi_height);

        cv::Mat cropped_frame =
            frame(cv::Rect(0, y_start, cfg_.frame_width, roi_height));

        HorizonLine vision_result = callVision(cropped_frame);

        if (!vision_result.is_estimated) {
            int   crop_center_y   = y_start + (roi_height / 2);
            int   image_center_y  = cfg_.frame_height / 2;
            float absolute_offset = (crop_center_y - image_center_y)
                                  + vision_result.offset;

            float disagreement   = std::abs(absolute_offset - kalman_.offset_y);
            float gate_threshold = 2.0f * kalman_.uncertainty_px() + 15.0f;

            if (!initialized_ || disagreement < gate_threshold) {
                kalman_.update(absolute_offset, kalman_.R_hailo);
                current_angle_   = vision_result.angle;
                last_hailo_time_ = now;
                initialized_     = true;
                got_vision       = true;
            }
        }
    }
#else
    // IMU only mode - silence unused-variable warnings
    (void)frame;
    (void)needs_vision;
    (void)seconds_since_hailo;
#endif

    if (!got_vision) {
        float roll_diff = imu.roll - prev_imu.roll;
        current_angle_ -= roll_diff;

        float imu_predicted_offset = imu.pitch * pixels_per_degree_;
        kalman_.update(imu_predicted_offset, kalman_.R_imu);

#if !HAS_VISION
        // Em modo IMU-only marcamos initialized depois da primeira frame
        // para nao ficar preso na flag !initialized_
        initialized_ = true;
#endif
    }

    {
        std::lock_guard<std::mutex> lock(imu_mutex_);
        last_imu_ = imu;
    }

    return { current_angle_, kalman_.offset_y, !got_vision };
}
