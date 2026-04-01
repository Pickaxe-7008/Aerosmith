#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

enum class AiMode {
    None = 0,
    Obstacle,
    Path,
    ManualScan,
};

inline const char* AiModeToCString(AiMode mode) {
    switch (mode) {
    case AiMode::Obstacle:
        return "OBSTACLE";
    case AiMode::Path:
        return "PATH";
    case AiMode::ManualScan:
        return "MANUAL_SCAN";
    case AiMode::None:
    default:
        return "NONE";
    }
}

struct AppState {
    std::atomic<bool> running{true};
    std::atomic<bool> drone_connected{false};
    std::atomic<int> ping_ms{999};
    std::atomic<AiMode> current_mode{AiMode::None};
    std::atomic<uint64_t> last_heartbeat_ms{0};
    std::atomic<uint64_t> last_frame_id{0};
    std::atomic<int> video_width{0};
    std::atomic<int> video_height{0};

    std::mutex frame_mutex;
    std::vector<uint8_t> latest_jpeg_buffer;
    bool new_frame_available = false;
    std::string video_codec;

    std::mutex inference_mutex;
    std::string latest_inference_json;
};

