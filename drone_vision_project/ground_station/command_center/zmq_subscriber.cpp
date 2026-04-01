#include "app_state.h"
#include "messages.pb.h"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stop_token>
#include <string>
#include <zmq.hpp>

static uint64_t NowMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

void DroneSubscriberThread(std::stop_token stop_token, zmq::context_t& context, AppState& app_state) {
    // 1. Create a SUB socket
    zmq::socket_t sub_socket(context, zmq::socket_type::sub);

    // 2. Bind to a port (e.g., 5060)
    // By BINDING on the ground station, the drone acts as the client.
    // This is ideal for LTE failover: the drone just looks for your laptop's public IP.
    sub_socket.bind("tcp://*:5060");

    sub_socket.set(zmq::sockopt::rcvtimeo, 200);
    sub_socket.set(zmq::sockopt::subscribe, "");

    std::cout << "[Command Center] ZMQ Subscriber listening on port 5060..." << std::endl;

    while (!stop_token.stop_requested() && app_state.running.load(std::memory_order_relaxed)) {
        zmq::message_t wire_msg;
        const auto recv_result = sub_socket.recv(wire_msg, zmq::recv_flags::none);
        if (!recv_result.has_value()) {
            continue;
        }

        aerosmith::v1::Envelope envelope;
        if (!envelope.ParseFromArray(wire_msg.data(), static_cast<int>(wire_msg.size()))) {
            std::cout << "[Command Center] Dropped invalid envelope from drone link" << std::endl;
            continue;
        }

        app_state.last_heartbeat_ms.store(NowMs(), std::memory_order_relaxed);
        app_state.drone_connected.store(true, std::memory_order_relaxed);

        if (envelope.msg_type() == aerosmith::v1::MESSAGE_TYPE_VIDEO_FRAME && envelope.has_video_frame()) {
            const auto& frame = envelope.video_frame();
            if (frame.codec() != "jpeg" && frame.codec() != "JPEG") {
                continue;
            }
            std::lock_guard<std::mutex> lock(app_state.frame_mutex);
            app_state.latest_jpeg_buffer.assign(frame.payload().begin(), frame.payload().end());
            app_state.new_frame_available = true;
            app_state.last_frame_id.store(frame.frame_id(), std::memory_order_relaxed);
            app_state.video_width.store(static_cast<int>(frame.width()), std::memory_order_relaxed);
            app_state.video_height.store(static_cast<int>(frame.height()), std::memory_order_relaxed);
            app_state.video_codec = frame.codec();
        } else if (envelope.msg_type() == aerosmith::v1::MESSAGE_TYPE_TELEMETRY && envelope.has_telemetry()) {
            const auto& telemetry = envelope.telemetry();
            app_state.ping_ms.store(static_cast<int>(telemetry.ping_ms()), std::memory_order_relaxed);
        } else if (envelope.msg_type() == aerosmith::v1::MESSAGE_TYPE_HEARTBEAT && envelope.has_heartbeat()) {
            // Heartbeats only refresh link health timers.
        }
    }
}