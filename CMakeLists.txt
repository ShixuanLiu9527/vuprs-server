cmake_minimum_required(VERSION 3.10)
project(vuprs)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-std=c++17)
endif()

if(CMAKE_CROSSCOMPILING)
    # 交叉编译时，使用针对ARM架构的优化
    add_compile_options(-O3 -march=armv8-a)
else()
    # 本地编译优化选项
    add_compile_options(-O3 -march=native)
endif()

add_subdirectory(eigen)
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/eigen
        ${CMAKE_CURRENT_SOURCE_DIR}
)

file(GLOB_RECURSE SOLVER_SRC 
        "source/*.cpp"
)

add_executable(fpga_tool fpga_tool.cpp ${SOLVER_SRC})
add_executable(vuprs_server main.cpp ${SOLVER_SRC})
