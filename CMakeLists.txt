cmake_minimum_required(VERSION 3.14)
project(AegisEdgeDetection CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------
# Build modes:
#   cmake .. -DWITH_HOUGH=ON    (OpenCV Hough — recommended for demo)
#   cmake .. -DWITH_HAILO=ON    (Hailo NPU — requires hailort)
#   cmake ..                    (IMU only — no vision)
# -----------------------------------------------------------------------

option(WITH_HOUGH "Enable Hough-based horizon detection" OFF)
option(WITH_HAILO "Enable Hailo NPU horizon detection"   OFF)

find_package(OpenCV REQUIRED)

add_executable(horizon_detection_test
    drone-sim/horizon_detection_test.cpp
    DetectionSystems/ohd.cpp
)

target_include_directories(horizon_detection_test PRIVATE
    DetectionSystems
)

target_link_libraries(horizon_detection_test PRIVATE
    ${OpenCV_LIBS}
)

if(WITH_HAILO)
    target_compile_definitions(horizon_detection_test PRIVATE WITH_HAILO)
    # Adjust if hailort is installed elsewhere
    find_library(HAILORT_LIB hailort REQUIRED)
    target_link_libraries(horizon_detection_test PRIVATE ${HAILORT_LIB})
    message(STATUS "Build mode: IMU + Hailo (NPU)")
elseif(WITH_HOUGH)
    target_compile_definitions(horizon_detection_test PRIVATE WITH_HOUGH)
    message(STATUS "Build mode: IMU + Hough (Classic CV)")
else()
    message(STATUS "Build mode: IMU only (no vision)")
endif()
