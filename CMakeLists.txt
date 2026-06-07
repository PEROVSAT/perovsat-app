cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cubesat_app)

# Enable C++17
set(CMAKE_CXX_STANDARD 17)

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(drivers/imu)

target_sources(app PRIVATE
    src/main.cpp
    src/system_health.cpp
    src/payload.cpp
    src/dfa.cpp
    src/communications.cpp
    src/commands.cpp
)
