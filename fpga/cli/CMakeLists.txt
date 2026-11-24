cmake_minimum_required(VERSION 3.10)
project(fpga_cli)

# Set C++ standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

get_filename_component(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)

if(CMAKE_CROSSCOMPILING)
    add_compile_options(-O3 -march=armv8-a)
else()
    add_compile_options(-O3 -march=native)
endif()

include_directories(
    ${PROJECT_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_ROOT}/3rdparty/
    ${PROJECT_ROOT}/3rdparty/eigen
    ${PROJECT_ROOT}/fpga
    ${PROJECT_ROOT}/misc
)

# Exclude this CLI's own sources (globbed in via fpga/*.cpp).
file(GLOB_RECURSE FPGA_SRC "${PROJECT_ROOT}/fpga/*.cpp")
list(FILTER FPGA_SRC EXCLUDE REGEX "/cli/")
file(GLOB_RECURSE MISC_SRC "${PROJECT_ROOT}/misc/*.cpp")

message(STATUS "FPGA_SRC: ${FPGA_SRC}")
message(STATUS "MISC_SRC: ${MISC_SRC}")

add_executable(tool main.cpp fpga_tool.cpp ${FPGA_SRC} ${MISC_SRC})

target_link_libraries(tool PRIVATE m Threads::Threads)
