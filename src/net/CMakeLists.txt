# 设置头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
# 设置源文件列表
# aux_source_directory(. SRC_LIST)
set(SRC_LIST 
    Channel.cc
    CurrentThread.cc
    InetAddress.cc
    Logger.cc
    Timestamp.cc)
add_library(myMuduo ${SRC_LIST})
target_link_libraries(myMuduo)