#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "app_state.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stop_token>
#include <thread>
#include <zmq.hpp>

// Forward declarations
void RenderDashboard(AppState& app_state);
void InferenceWorkerThread(std::stop_token stop_token, zmq::context_t& context, AppState& app_state);
void DroneSubscriberThread(std::stop_token stop_token, zmq::context_t& context, AppState& app_state);

static uint64_t NowMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

int main() {
    AppState app_state;
    zmq::context_t zmq_context(1);
    std::jthread ai_thread(InferenceWorkerThread, std::ref(zmq_context), std::ref(app_state));
    std::jthread drone_thread(DroneSubscriberThread, std::ref(zmq_context), std::ref(app_state));

    // Setup Window (GLFW)
    if (!glfwInit()) return -1;
    const char* glsl_version = "#version 130";
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Drone Command Center", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const uint64_t last_heartbeat = app_state.last_heartbeat_ms.load(std::memory_order_relaxed);
        if (last_heartbeat == 0 || (NowMs() - last_heartbeat) > 2000) {
            app_state.drone_connected.store(false, std::memory_order_relaxed);
        }

        // Start new ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Draw our custom UI
        RenderDashboard(app_state);

        // Render
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Dark grey background
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    app_state.running.store(false, std::memory_order_relaxed);
    ai_thread.request_stop();
    drone_thread.request_stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}