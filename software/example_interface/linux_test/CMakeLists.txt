cmake_minimum_required(VERSION 3.1.2)
project(example_interface_lxtest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(-DLLCP_LITTLE_ENDIAN)

add_definitions(-Wall)
add_definitions(-Wextra)
add_definitions(-Wpedantic)
# add_definitions(-O0)
# add_definitions(-g)

include_directories(
  include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../llcp/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../mui/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../serial_port
)

add_executable(example_interface_lxtest
  src/main.cpp

  src/minipix_interface_linux.cpp
  src/gatherer_interface_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../serial_port/serial_port.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/gatherer_interface.c

  ${CMAKE_CURRENT_SOURCE_DIR}/../../mui/src/mui.c

  ${CMAKE_CURRENT_SOURCE_DIR}/../../serial_port/serial_port.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/../../llcp/src/llcp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../llcp/src/llcp_endian.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../llcp/src/llcp_minipix_messages.c
)

target_compile_definitions(example_interface_lxtest PRIVATE MUI_SEND_STRING=1)
# target_compile_definitions(example_interface_lxtest PRIVATE MUI_SEND_CHAR=1)

target_compile_definitions(example_interface_lxtest PRIVATE GATHERER_SEND_STRING=1)
# target_compile_definitions(example_interface_lxtest PRIVATE GATHERER_SEND_CHAR=1)

target_compile_definitions(example_interface_lxtest PRIVATE LLCP_DEBUG_PRINT=0)
