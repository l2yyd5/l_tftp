cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 17)
project(l_tftp)

set(SOURCES
    src/TFTPClient.cpp
    src/UDPClient.cpp
    TFTP_Client.cpp
)
find_package(Boost REQUIRED COMPONENTS)
if(NOT Boost_FOUND)
    message("Not found Boost")
endif()
 
include_directories(${Boost_INCLUDE_DIRS})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin/)

include_directories(include/)

add_executable(l_tftp ${SOURCES})
target_link_libraries(l_tftp ${Boost_LIBRARIES})