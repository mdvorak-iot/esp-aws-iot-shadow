cmake_minimum_required(VERSION 3.11.0)

idf_component_register(
        SRCS
        src/aws_iot_shadow.c
        src/aws_iot_shadow_mqtt_error.c
        INCLUDE_DIRS include
        REQUIRES freertos esp_common log mqtt
)
