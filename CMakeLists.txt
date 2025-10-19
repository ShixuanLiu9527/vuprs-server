cmake_minimum_required(VERSION 3.10)
project(vuprs)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-std=c++17)
endif()

if(CMAKE_CROSSCOMPILING)
    add_compile_options(-O3 -march=armv8-a)
else()
    add_compile_options(-O3 -march=native)
endif()

add_subdirectory(eigen)
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/fpga/include
        ${CMAKE_CURRENT_SOURCE_DIR}/server/include
        ${CMAKE_CURRENT_SOURCE_DIR}/network/include
        ${CMAKE_CURRENT_SOURCE_DIR}/logger/include
        ${CMAKE_CURRENT_SOURCE_DIR}/algorithm/include
        ${CMAKE_CURRENT_SOURCE_DIR}/system_tools/include
        ${CMAKE_CURRENT_SOURCE_DIR}/eigen
        ${CMAKE_CURRENT_SOURCE_DIR}
)

file(GLOB_RECURSE FPGA_SRC "fpga/source/*.cpp")
file(GLOB_RECURSE NETWORK_SRC "network/source/*.cpp")
file(GLOB_RECURSE SYS_TOOL_SRC "system_tools/source/*.cpp")
file(GLOB_RECURSE LOGGER_SRC "logger/source/*.cpp")
file(GLOB_RECURSE ALGORITHM_SRC "algorithm/source/*.cpp")
file(GLOB_RECURSE SERVER_SRC "server/source/*.cpp")

add_executable(fpga_tool fpga_tool.cpp ${FPGA_SRC} ${SYS_TOOL_SRC} ${LOGGER_SRC})
add_executable(vuprs_server test_server.cpp ${NETWORK_SRC} ${SYS_TOOL_SRC} ${LOGGER_SRC})
add_executable(sampling_test test_sampling.cpp ${FPGA_SRC} ${SYS_TOOL_SRC} ${LOGGER_SRC})

target_link_libraries(vuprs_server PRIVATE Threads::Threads)
