# 设置源文件列表
set(CALLER_LIST caller.cc example.pb.cc)
set(CALLEE_LIST callee.cc example.pb.cc)

add_executable(caller ${CALLER_LIST})
add_executable(callee ${CALLEE_LIST})

target_link_libraries(caller RPC protobuf)
target_link_libraries(callee RPC protobuf)