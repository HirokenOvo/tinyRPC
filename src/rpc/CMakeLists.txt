# 设置源文件列表
aux_source_directory(. SRC_LIST)
add_library(RPC ${SRC_LIST})
target_include_directories(RPC PUBLIC ./include)

target_link_libraries(RPC PUBLIC Comm Net zookeeper_mt)