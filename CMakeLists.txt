# CMake minimum version requirement
cmake_minimum_required(VERSION 3.10)

# Project name
project(server)

# Add C++14 support
set(CMAKE_CXX_STANDARD 14)

# 设置编译选项为debug模式
set(CMAKE_BUILD_TYPE Debug)

# 添加子目录
add_subdirectory(code)

# 指定生成目标
add_executable(${PROJECT_NAME} code/main.cpp)

# 添加链接库
target_link_libraries(${PROJECT_NAME} code pthread mysqlclient) 