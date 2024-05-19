cmake_minimum_required(VERSION 3.4)
project(udp-ping)
add_executable(udpServer udpServer.cpp)
set_target_properties(udpServer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/")

find_package(Boost REQUIRED COMPONENTS program_options)
add_executable(udpClient udpClient.cpp)
set_target_properties(udpClient PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/")
target_link_libraries(udpClient Boost::program_options)
