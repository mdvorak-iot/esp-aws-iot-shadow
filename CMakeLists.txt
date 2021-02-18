cmake_minimum_required(VERSION 3.11.0)

idf_component_register(
    SRC_DIRS src
    PRIV_INCLUDE_DIRS src/include
    INCLUDE_DIRS include
    REQUIRES freertos esp_common log json mqtt
)
