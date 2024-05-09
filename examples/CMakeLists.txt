add_subdirectory(EchoRPC)
add_subdirectory(bench)

# EchoServer
set(ECHOSERVER_LIST EchoServer.cc)
add_executable(EchoServer ${ECHOSERVER_LIST})
target_link_libraries(EchoServer RPC protobuf)
