cmake_minimum_required(VERSION 3.11.0)
include(ExternalComponent.cmake)

# In-place use of library in this example
list(APPEND EXTRA_COMPONENT_DIRS " ${CMAKE_SOURCE_DIR}/..")

# NOTE these dependencies are for example only
ExternalComponent_Get(wifi_reconnect https://github.com/mdvorak-iot/esp-wifi-reconnect.git e41d126)

# Project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(example)
