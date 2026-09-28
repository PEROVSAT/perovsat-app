cmake_minimum_required(VERSION 3.20.0)

set(SNIPPET_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Pico boards: default west flash/ninja flash to UF2 (BOOTSEL drag-and-drop).
if(BOARD MATCHES "^rpi_pico")
	set(BOARD_FLASH_RUNNER uf2)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cubesat_app)

# Enable C++17
set(CMAKE_CXX_STANDARD 17)

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(app PRIVATE
    src/main.cpp
    src/syshealth/system_health.cpp
    src/syshealth/watchdog.cpp
    src/payload/hw.cpp
    src/payload/payload.cpp
    src/payload/PayloadReading.cpp
    src/dfa/dfa.cpp
    src/communications/communications.cpp
    src/commands/commands.cpp
)
