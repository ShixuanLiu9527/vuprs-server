cmake_minimum_required(VERSION 3.10)
project(vuprs)

# Set C++ standard

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lm")
find_package(Threads REQUIRED)

if(CMAKE_CROSSCOMPILING)
    add_compile_options(-O3 -march=armv8-a)
else()
    add_compile_options(-O3 -march=native)
endif()

add_subdirectory(3rdparty/eigen)
add_subdirectory(3rdparty/fftw3)
add_subdirectory(fpga_tool)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/nlohmann
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/eigen
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fftw3
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fftw3/api
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/rknpu2/librknn_api/include
    ${CMAKE_CURRENT_SOURCE_DIR}/fpga
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${CMAKE_CURRENT_SOURCE_DIR}/logger
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithm/bf
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithm/signal_processing
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithm/inference
    ${CMAKE_CURRENT_SOURCE_DIR}/hybrid
    ${CMAKE_CURRENT_SOURCE_DIR}/system_tools
)

file(GLOB_RECURSE FPGA_SRC "${CMAKE_CURRENT_SOURCE_DIR}/fpga/*.cpp")
file(GLOB_RECURSE SYS_TOOL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/system_tools/*.cpp")
file(GLOB_RECURSE LOGGER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/logger/*.cpp")
file(GLOB_RECURSE ALGORITHM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/algorithm/*.cpp")
file(GLOB_RECURSE SERVER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/server/*.cpp")
file(GLOB_RECURSE HYBRID_BF_SRC "${CMAKE_CURRENT_SOURCE_DIR}/hybrid/*.cpp")
file(GLOB_RECURSE FAULT_DETECT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/fault_detect/*.cpp")

add_executable(server main.cpp 
    ${FPGA_SRC} 
    ${SYS_TOOL_SRC} 
    ${LOGGER_SRC} 
    ${ALGORITHM_SRC} 
    ${SERVER_SRC} 
    ${HYBRID_BF_SRC}
    ${FAULT_DETECT_SRC})
target_link_directories(server PRIVATE 
${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/rknpu2/librknn_api/aarch64)
target_link_libraries(server PRIVATE Threads::Threads m fftw3 rknnrt)
