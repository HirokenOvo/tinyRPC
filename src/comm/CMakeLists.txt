# 设置源文件列表
aux_source_directory(. SRC_LIST)
add_library(Comm ${SRC_LIST})
target_link_libraries(Comm)