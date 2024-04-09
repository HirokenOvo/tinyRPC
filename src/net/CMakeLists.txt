# 设置源文件列表
aux_source_directory(. SRC_LIST)
# set(SRC_LIST 
#     Channel.cc
#     CurrentThread.cc
#     EpollPoller.cc
#     EventLoop.cc
#     InetAddress.cc
#     Logger.cc
#     Timestamp.cc)
add_library(Net ${SRC_LIST})

target_include_directories(Net PUBLIC ./include)

target_link_libraries(Net PUBLIC Comm)