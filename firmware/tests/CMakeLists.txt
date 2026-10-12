cmake_minimum_required(VERSION 3.22)

# Native unit test build configuration
project(water_quality_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/config
    .
)

# Math library
find_library(MATH_LIB m)

# ------------------------------------------------------------------------------
# Test: EKF and Anomaly Detector
# ------------------------------------------------------------------------------
add_executable(test_ekf
    test_ekf.c
    ../src/ekf/ekf.c
    ../src/ekf/water_quality_ekf.c
    ../src/ekf/anomaly_detector.c
)
if(MATH_LIB)
    target_link_libraries(test_ekf ${MATH_LIB})
endif()
add_test(NAME test_ekf COMMAND test_ekf)

# ------------------------------------------------------------------------------
# Test: Sensor Drivers (pH, Turbidity, DO)
# ------------------------------------------------------------------------------
add_executable(test_sensors
    test_sensors.c
    ../src/sensors/ph_sensor.c
    ../src/sensors/turbidity_sensor.c
    ../src/sensors/do_sensor.c
)
if(MATH_LIB)
    target_link_libraries(test_sensors ${MATH_LIB})
endif()
add_test(NAME test_sensors COMMAND test_sensors)

# ------------------------------------------------------------------------------
# Test: Packet Serialization & Decoding
# ------------------------------------------------------------------------------
add_executable(test_packet
    test_packet.c
    ../src/comms/packet.c
)
if(MATH_LIB)
    target_link_libraries(test_packet ${MATH_LIB})
endif()
add_test(NAME test_packet COMMAND test_packet)
