# 设置源文件列表
set(CALLER_LIST caller.cc example.pb.cc)
set(CALLEE_LIST callee.cc example.pb.cc)

add_executable(Caller ${CALLER_LIST})
add_executable(Callee ${CALLEE_LIST})

target_link_libraries(Caller RPC protobuf)
target_link_libraries(Callee RPC protobuf)