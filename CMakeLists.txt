# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16.0)

# This example uses an extra component for common functions such as Wi-Fi and console.
list(APPEND EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/system/console/advanced/components/cmd_system)
# list(APPEND EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/system/console/advanced/components/cmd_nvs)
# list(APPEND EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/system/console/advanced/components/cmd_wifi)
list(APPEND EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/components/console)
list(APPEND EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/components/driver)
list(APPEND EXTRA_COMPONENT_DIRS esp-iot-solution/examples/indicator/components/cmd_led_indicator)
list(APPEND EXTRA_COMPONENT_DIRS esp-iot-solution/components/led/led_indicator)

find_package(Git)

execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(PROJECT_VER "${VERSION}")
message(STATUS "APP_VERSION: ${VERSION}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(dirty_usb_disconnect)
