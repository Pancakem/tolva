# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
include_directories(include)
project(tolva)

FILE(GLOB app_sources src/*.c)


target_sources(app PRIVATE
  ${app_sources}
  )

# Optionally, include the tests directory if testing
if(CONFIG_TESTING)
    add_subdirectory(tests)
endif()
