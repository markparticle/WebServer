# 在code目录中的CMakeLists.txt文件
file(GLOB_RECURSE SOURCES "log/*.cpp" "pool/*.cpp" "timer/*.cpp"
                          "http/*.cpp" "server/*.cpp" "buffer/*.cpp" "main.cpp")
add_library(code ${SOURCES})