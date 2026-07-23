cmake_minimum_required(VERSION 3.10)
project(fpga_tool)

# Set C++ standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

get_filename_component(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)

if(CMAKE_CROSSCOMPILING)
    add_compile_options(-O3 -march=armv8-a)
else()
    add_compile_options(-O3 -march=native)
endif()

include_directories(
    ${PROJECT_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_ROOT}/3rdparty/
    ${PROJECT_ROOT}/3rdparty/eigen
    ${PROJECT_ROOT}/fpga/include
    ${PROJECT_ROOT}/system_tools/include
)

file(GLOB_RECURSE FPGA_TOOL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
file(GLOB_RECURSE FPGA_SRC "${PROJECT_ROOT}/fpga/source/*.cpp")
file(GLOB_RECURSE SYS_TOOL_SRC "${PROJECT_ROOT}/system_tools/source/*.cpp")

message(STATUS "FPGA_TOOL_SRC: ${FPGA_TOOL_SRC}")
message(STATUS "FPGA_SRC: ${FPGA_SRC}")
message(STATUS "SYS_TOOL_SRC: ${SYS_TOOL_SRC}")

add_executable(tool main.cpp ${FPGA_TOOL_SRC} ${FPGA_SRC} ${SYS_TOOL_SRC})

target_link_libraries(tool PRIVATE m Threads::Threads)
