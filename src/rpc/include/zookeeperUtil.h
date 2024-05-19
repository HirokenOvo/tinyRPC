#pragma once

#include <zookeeper/zookeeper.h>
#include <string>
#include <map>
#include "uncopyable.h"

template <typename K, typename V>
class WriteFstMp;

class ZkClient : uncopyable
{
public:
    ZkClient();
    ~ZkClient();
    void start(ZooLogLevel logLevel);
    // 在zkserver上根据指定的path创建znode节点
    void create(const char *path, const char *data, int datalen, int state = 0);
    // 根据参数指定的znode节点路径获得znode的值
    std::string getData(const char *path);
    // FIXME:添加cache
    std::string getData(const char *path, WriteFstMp<std::string, struct String_vector> *);
    void getChildrenList(const char *path, void *watcherCtx);
    void close();
    bool exist();

private:
    zhandle_t *zhandle_;
};