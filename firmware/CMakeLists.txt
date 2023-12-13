cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(BOARD ble_pulse_encoder)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ble-pulse-encoder)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
