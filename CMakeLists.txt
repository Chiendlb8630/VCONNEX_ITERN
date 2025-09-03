# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Vconnex_switch)

set(EXTRA_COMPONENT_DIRS components/espressif__led_strip)
set(EXTRA_COMPONENT_DIRS components/iic_switch)
set(EXTRA_COMPONENT_DIRS components/led_buzzer_sw)
set(EXTRA_COMPONENT_DIRS components/blufi_app)
set(EXTRA_COMPONENT_DIRS components/mqtt_app)
set(EXTRA_COMPONENT_DIRS components/app_time)
set(EXTRA_COMPONENT_DIRS components/app_ble_mesh)
set(EXTRA_COMPONENT_DIRS components/app_ota)