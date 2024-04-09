# cmake版本最低要求
cmake_minimum_required(VERSION 3.5)  
# 设置工程名称
project(tinyRPC)  

# 指定 C++ 版本
set(CMAKE_CXX_STANDARD 20)  
# 调试使用Debug，可以查看中间变量；发布使用Release，运行速度快
set(CMAKE_BUILD_TYPE "Debug")  

# message("${PROJECT_SOURCE_DIR}=" ${PROJECT_SOURCE_DIR})

# 设置可执行文件的输出目录
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)  
# 设置库文件的输出目录
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# include_directories(${PROJECT_SOURCE_DIR}/src/comm/include
#                     ${PROJECT_SOURCE_DIR}/src/net/include
#                     ${PROJECT_SOURCE_DIR}/src/rpc/include
# )

# 添加 myMuduo 子项目
add_subdirectory(src)
add_subdirectory(examples)
