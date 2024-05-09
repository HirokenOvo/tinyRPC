set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/bench)
# 设置源文件列表
set(client_LIST client.cc echo.pb.cc)
set(server_LIST server.cc echo.pb.cc)

add_executable(client ${client_LIST})
add_executable(server ${server_LIST})

target_link_libraries(client RPC protobuf)
target_link_libraries(server RPC protobuf)

# 将 server 可执行文件输出到 bin/bench 目录下
set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}")

# 将 client 可执行文件输出到 bin/bench/client 目录下
set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}/client")