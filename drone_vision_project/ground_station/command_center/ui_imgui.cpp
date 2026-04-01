#include "imgui.h"
#include "app_state.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

#ifdef AEROSMITH_HAS_OPENCV
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

static bool UploadJpegToTexture(const std::vector<uint8_t>& jpeg_bytes, GLuint& texture_id, int& width, int& height,
                                std::string& status) {
#ifdef AEROSMITH_HAS_OPENCV
    if (jpeg_bytes.empty()) {
        status = "No JPEG bytes available";
        return false;
    }

    cv::Mat encoded(1, static_cast<int>(jpeg_bytes.size()), CV_8UC1,
                    const_cast<uint8_t*>(jpeg_bytes.data()));
    cv::Mat bgr = cv::imdecode(encoded, cv::IMREAD_COLOR);
    if (bgr.empty()) {
        status = "JPEG decode failed";
        return false;
    }

    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);

    if (texture_id == 0) {
        glGenTextures(1, &texture_id);
    }

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
    glBindTexture(GL_TEXTURE_2D, 0);

    width = rgb.cols;
    height = rgb.rows;
    status.clear();
    return true;
#else
    (void)jpeg_bytes;
    (void)texture_id;
    (void)width;
    (void)height;
    status = "OpenCV not linked; install OpenCV and reconfigure CMake to render JPEG frames";
    return false;
#endif
}

void RenderDashboard(AppState& app_state) {
            static GLuint video_texture_id = 0;
            static int texture_width = 0;
            static int texture_height = 0;
            static std::string video_status = "Waiting for first frame";

            std::vector<uint8_t> jpeg_snapshot;
            {
                std::lock_guard<std::mutex> lock(app_state.frame_mutex);
                if (app_state.new_frame_available) {
                    jpeg_snapshot = app_state.latest_jpeg_buffer;
                    app_state.new_frame_available = false;
                }
            }

            if (!jpeg_snapshot.empty()) {
                UploadJpegToTexture(jpeg_snapshot, video_texture_id, texture_width, texture_height, video_status);
            }

    const bool is_connected = app_state.drone_connected.load(std::memory_order_relaxed);
    const int ping_ms = app_state.ping_ms.load(std::memory_order_relaxed);
    const AiMode mode = app_state.current_mode.load(std::memory_order_relaxed);

    // 1. Control Panel Window
    ImGui::Begin("Flight Control & Telemetry");

    ImGui::Text("System Status:");
    if (is_connected) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "DRONE CONNECTED (%d ms)", ping_ms);
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "LINK OFFLINE");
    }

    ImGui::Separator();

    ImGui::Text("AI Detection Mode:");
    if (ImGui::Button("Obstacle Avoidance")) app_state.current_mode.store(AiMode::Obstacle, std::memory_order_relaxed);
    ImGui::SameLine();
    if (ImGui::Button("Path Segmentation")) app_state.current_mode.store(AiMode::Path, std::memory_order_relaxed);

    ImGui::Separator();

    // Simulate sending the trigger manually from the UI
    if (ImGui::Button("FORCE MANUAL SCAN", ImVec2(-1, 50))) {
        app_state.current_mode.store(AiMode::ManualScan, std::memory_order_relaxed);
    }

    ImGui::End();

    // 2. Main Video / Scan Feed Window
    ImGui::Begin("Scan Feed / AI Vision");
    ImGui::Text("Active Mode: %s", AiModeToCString(mode));
    ImGui::Text("Frame ID: %llu", static_cast<unsigned long long>(app_state.last_frame_id.load(std::memory_order_relaxed)));
    ImGui::Text("Codec: %s", app_state.video_codec.empty() ? "unknown" : app_state.video_codec.c_str());
    ImGui::Text("Source Resolution: %d x %d", app_state.video_width.load(std::memory_order_relaxed),
                app_state.video_height.load(std::memory_order_relaxed));

    {
        std::lock_guard<std::mutex> lock(app_state.inference_mutex);
        if (!app_state.latest_inference_json.empty()) {
            ImGui::TextWrapped("Latest Inference: %s", app_state.latest_inference_json.c_str());
        }
    }

    if (video_texture_id != 0 && texture_width > 0 && texture_height > 0) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float sx = avail.x / static_cast<float>(texture_width);
        const float sy = avail.y / static_cast<float>(texture_height);
        const float scale = std::max(0.1f, std::min(sx, sy));
        ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(video_texture_id)),
                     ImVec2(texture_width * scale, texture_height * scale));
    } else {
        ImGui::TextWrapped("Video unavailable: %s", video_status.c_str());
    }

    ImGui::End();
}