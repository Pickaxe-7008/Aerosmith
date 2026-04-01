cmake_minimum_required(VERSION 3.15)
project(CommandCenter)

set(CMAKE_CXX_STANDARD 20)

# Find Dependencies
find_package(glfw3 REQUIRED)
find_package(cppzmq REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenCV QUIET COMPONENTS imgcodecs imgproc)

# Add Dear ImGui sources (assuming you downloaded them into an 'imgui' subfolder)
set(IMGUI_SOURCES
    imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(command_center
    main.cpp
    ui_imgui.cpp
    zmq_subscriber.cpp
    zmq_dealer.cpp
    ../../common/compiled/cpp/messages.pb.cc
    ${IMGUI_SOURCES}
)

target_link_libraries(command_center PRIVATE glfw cppzmq protobuf::libprotobuf Threads::Threads)
target_include_directories(command_center PRIVATE imgui imgui/backends ../../common ../../common/compiled/cpp)

if (OpenCV_FOUND)
    target_link_libraries(command_center PRIVATE ${OpenCV_LIBS})
    target_include_directories(command_center PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(command_center PRIVATE AEROSMITH_HAS_OPENCV=1)
endif()

