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

add_subdirectory(eigen)
add_subdirectory(fftw3)
add_subdirectory(fpga_tool)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/fpga/include
    ${CMAKE_CURRENT_SOURCE_DIR}/server/include
    ${CMAKE_CURRENT_SOURCE_DIR}/logger/include
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithm/include
    ${CMAKE_CURRENT_SOURCE_DIR}/collab_bf/include
    ${CMAKE_CURRENT_SOURCE_DIR}/system_tools/include
    ${CMAKE_CURRENT_SOURCE_DIR}/eigen
    ${CMAKE_CURRENT_SOURCE_DIR}/fftw3
    ${CMAKE_CURRENT_SOURCE_DIR}/fftw3/api
)

file(GLOB_RECURSE FPGA_SRC "${CMAKE_CURRENT_SOURCE_DIR}/fpga/source/*.cpp")
file(GLOB_RECURSE SYS_TOOL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/system_tools/source/*.cpp")
file(GLOB_RECURSE LOGGER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/logger/source/*.cpp")
file(GLOB_RECURSE ALGORITHM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/algorithm/source/*.cpp")
file(GLOB_RECURSE SERVER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/server/source/*.cpp")
file(GLOB_RECURSE COLLAB_BF_SRC "${CMAKE_CURRENT_SOURCE_DIR}/collab_bf/source/*.cpp")

add_executable(server main.cpp ${FPGA_SRC} ${SYS_TOOL_SRC} ${LOGGER_SRC} ${ALGORITHM_SRC} ${SERVER_SRC} ${COLLAB_BF_SRC})

target_link_libraries(server PRIVATE Threads::Threads m fftw3)
