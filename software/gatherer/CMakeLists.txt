cmake_minimum_required(VERSION 3.1.2)
project(gatherer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set this to 1 to compile with OpenCV
set(GUI 0)

if(${GUI})
  MESSAGE(WARNING "[Gatherer]: Compiling with GUI")
else()
  MESSAGE(WARNING "[Gatherer]: Compiling without GUI")
endif()

add_definitions(-DLLCP_LITTLE_ENDIAN)

add_definitions(-Wall)
add_definitions(-Wextra)
add_definitions(-Wpedantic)
# add_definitions(-O0)
# add_definitions(-g)

find_package(Threads REQUIRED) # only for linux

set(INCLUDE_DIRS
  include
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../serial_port
  )

set(LINK_LIBS
  Threads::Threads # only for linux
  )

## | ------------ add OpenCV GUI >>>>>>>>>>>>>>>>> ------------ |
if(${GUI})

find_package(OpenCV REQUIRED)

set(INCLUDE_DIRS
  ${INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
)

set(LINK_LIBS
  ${LINK_LIBS}
  ${OpenCV_LIBS}
)

endif()
## | ------------ add OpenCV GUI <<<<<<<<<<<<<<<<< ------------ |

add_executable(gatherer_tot_toa
  src/main_tot_toa.cpp
  src/gatherer.cpp
  src/pixel_decoder.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../serial_port/serial_port.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp_endian.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp_minipix_messages.c
)

target_link_libraries(gatherer_tot_toa
  ${LINK_LIBS}
)

include_directories(
  ${INCLUDE_DIRS} 
)

target_compile_definitions(gatherer_tot_toa PRIVATE GUI=${GUI})


add_executable(gatherer
  src/main.cpp
  src/gatherer.cpp
  src/pixel_decoder.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../serial_port/serial_port.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp_endian.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../llcp/src/llcp_minipix_messages.c
)

target_link_libraries(gatherer
  ${LINK_LIBS}
)

target_compile_definitions(gatherer PRIVATE GUI=${GUI})
