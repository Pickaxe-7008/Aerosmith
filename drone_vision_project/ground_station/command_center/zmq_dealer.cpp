#include "app_state.h"
#include "messages.pb.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <stop_token>
#include <string>
#include <thread>
#include <zmq.hpp>

static aerosmith::v1::ScanMode ToProtoMode(AiMode mode) {
    switch (mode) {
    case AiMode::Obstacle:
        return aerosmith::v1::SCAN_MODE_OBSTACLE;
    case AiMode::Path:
        return aerosmith::v1::SCAN_MODE_PATH;
    case AiMode::ManualScan:
        return aerosmith::v1::SCAN_MODE_MANUAL_SCAN;
    case AiMode::None:
    default:
        return aerosmith::v1::SCAN_MODE_NONE;
    }
}

static std::string BuildInferenceSummary(const aerosmith::v1::InferenceResult& result) {
    std::ostringstream oss;
    oss << "frame=" << result.frame_id() << " latency_ms=" << result.inference_latency_ms()
        << " detections=" << result.detections_size();
    if (result.detections_size() > 0) {
        const auto& first = result.detections(0);
        oss << " first=" << first.label() << "(" << first.confidence() << ")";
    }
    return oss.str();
}

void InferenceWorkerThread(std::stop_token stop_token, zmq::context_t& context, AppState& app_state) {
    // DEALER socket talks to the Python local worker
    zmq::socket_t ai_socket(context, zmq::socket_type::dealer);
    ai_socket.bind("tcp://127.0.0.1:5050");
    ai_socket.set(zmq::sockopt::rcvtimeo, 50);
    ai_socket.set(zmq::sockopt::sndtimeo, 50);

    std::cout << "[Command Center] ZMQ Dealer listening on port 5050..." << std::endl;
    AiMode last_sent_mode = AiMode::None;

    while (!stop_token.stop_requested() && app_state.running.load(std::memory_order_relaxed)) {
        const AiMode current_mode = app_state.current_mode.load(std::memory_order_relaxed);
        if (current_mode != last_sent_mode && current_mode != AiMode::None) {
            aerosmith::v1::Envelope envelope;
            envelope.set_schema_version(1);
            envelope.set_msg_type(aerosmith::v1::MESSAGE_TYPE_CONTROL_COMMAND);
            envelope.set_timestamp_ms(static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count()));
            auto* command = envelope.mutable_control_command();
            command->set_requested_mode(ToProtoMode(current_mode));
            command->set_trigger_manual_scan(current_mode == AiMode::ManualScan);
            command->set_command_id(envelope.timestamp_ms());

            std::string outbound;
            if (envelope.SerializeToString(&outbound)) {
                ai_socket.send(zmq::buffer(outbound), zmq::send_flags::none);
            }
            last_sent_mode = current_mode;
        }

        // Wait for a result from the Python AI worker
        zmq::message_t reply;
        auto res = ai_socket.recv(reply, zmq::recv_flags::none);

        if (res.has_value()) {
            aerosmith::v1::Envelope envelope;
            if (!envelope.ParseFromArray(reply.data(), static_cast<int>(reply.size()))) {
                continue;
            }

            if (envelope.msg_type() == aerosmith::v1::MESSAGE_TYPE_INFERENCE_RESULT && envelope.has_inference_result()) {
                const auto& inference = envelope.inference_result();
                const std::string summary = BuildInferenceSummary(inference);
                {
                    std::lock_guard<std::mutex> lock(app_state.inference_mutex);
                    app_state.latest_inference_json = summary;
                }
                std::cout << "[AI Result]: " << summary << std::endl;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}