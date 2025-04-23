cmake_minimum_required(VERSION 3.1.2)
project(example_oneweb)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED) # only for linux

add_definitions(-DLLCP_LITTLE_ENDIAN)

add_definitions(-Wall)
add_definitions(-Wextra)
add_definitions(-Wpedantic)
add_definitions(-O0)
add_definitions(-g)

include_directories(
  include
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../mui/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../serial_port
)

add_executable(example_oneweb
  src/main.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/../serial_port/serial_port.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/../mui/src/mui.c

  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp_endian.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp_minipix_messages.c
)

target_link_libraries(example_oneweb
  Threads::Threads # only for linux
)

target_compile_definitions(example_oneweb PRIVATE MUI_SEND_STRING=1)
# target_compile_definitions(example_oneweb PRIVATE MUI_SEND_CHAR=1)

# target_compile_definitions(example_oneweb PRIVATE LLCP_DEBUG_PRINT=1)
