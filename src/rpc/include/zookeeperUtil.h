#pragma once

#include <zookeeper/zookeeper.h>
#include <string>
class ZkClient
{
public:
    ZkClient();
    ~ZkClient();
    void start(ZooLogLevel logLevel);
    // 在zkserver上根据指定的path创建znode节点
    void create(const char *path, const char *data, int datalen, int state = 0);
    // 根据参数指定的znode节点路径获得znode的值
    std::string getData(const char *path);

private:
    zhandle_t *zhandle_;
};