cmake_minimum_required(VERSION 3.1.2)
project(mui)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(-DLLCP_LITTLE_ENDIAN)

add_definitions(-Wall)
add_definitions(-Wextra)
add_definitions(-Wpedantic)
# add_definitions(-O0)
# add_definitions(-g)

include_directories(
  include
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/include
)

add_library(mui
  src/mui.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp_endian.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp_minipix_messages.c
)

target_compile_definitions(mui PRIVATE MUI_SEND_STRING=1)
# target_compile_definitions(mui PRIVATE MUI_SEND_CHAR=1)
