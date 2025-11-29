cmake_minimum_required(VERSION 3.1.2)
project(example_spi)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(-DLLCP_LITTLE_ENDIAN)

add_definitions(-Wall)
add_definitions(-Wextra)
add_definitions(-Wpedantic)
add_definitions(-O0)
add_definitions(-g)

find_package(PkgConfig REQUIRED)

pkg_check_modules(FTDI REQUIRED libftdi1)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

include_directories(
  include
  ${FTDI_INCLUDE_DIRS}
  ${LIBUSB_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../mui/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../spi
)

link_directories(
  ${FTDI_LIBRARY_DIRS}
  ${LIBUSB_LIBRARY_DIRS}
)

add_executable(example_spi
  src/main.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/../spi/spi_port.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/../mui/src/mui.c

  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp_endian.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp_minipix_messages.c
)

target_link_libraries(example_spi
  ${FTDI_LIBRARIES}
  ${LIBUSB_LIBRARIES}
)

target_compile_definitions(example_spi PRIVATE MUI_SEND_STRING=1)
target_compile_definitions(example_spi PRIVATE MUI_USER_HANDSHAKES=1)

# target_compile_definitions(example_spi PRIVATE LLCP_DEBUG_PRINT=1)
