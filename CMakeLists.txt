cmake_minimum_required(VERSION 3.11.0)

include(${CMAKE_CURRENT_LIST_DIR}/device_shadow_for_aws/shadowFilePaths.cmake)

set(SHADOW_COMPONENT_SOURCES src/esp_aws_shadow.c)

idf_component_register(
    SRCS ${SHADOW_SOURCES} ${SHADOW_COMPONENT_SOURCES}
    INCLUDE_DIRS ${SHADOW_INCLUDE_PUBLIC_DIRS} include
    REQUIRES freertos esp_common log json mqtt mbedtls
)
