#include "zookeeperUtil.h"
#include "Config.h"
#include "Logger.h"
#include "WriteFstMp.h"

#include <semaphore.h>

static void getChildrenListwatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);

void global_watcher(zhandle_t *zh, int type,
                    int state, const char *path, void *watcherCtx)
{
    if (type == ZOO_SESSION_EVENT) // 回调的消息类型是和会话相关的消息类型
    {
        if (state == ZOO_CONNECTED_STATE) // zkclient和zkserver连接成功
        {
            sem_t *sem = static_cast<sem_t *>(const_cast<void *>(zoo_get_context(zh)));
            sem_post(sem);
        }
    }
}
ZkClient::ZkClient() : zhandle_{nullptr} {}

ZkClient ::~ZkClient()
{
    if (zhandle_ != nullptr)
        zookeeper_close(zhandle_);
}

void ZkClient::start(ZooLogLevel logLevel)
{
    zoo_set_debug_level(logLevel);
    std::string ip = Config::getCfg("zk_ip");
    std::string port = Config::getCfg("zk_port");
    std::string ipPort = ip + ":" + port;

    /*
    zookeeper_mt：多线程版本
    zookeeper的API客户端程序提供了三个线程
    API调用线程
    网络I/O线程  pthread_create  poll
    watcher回调线程 pthread_create
    */
    zhandle_ = zookeeper_init(ipPort.c_str(), global_watcher, 5000, nullptr, nullptr, 0);

    if (zhandle_ == nullptr)
    {
        LOG_ERROR("zookeeper init error!");
        exit(EXIT_FAILURE);
    }

    sem_t sem;
    sem_init(&sem, 0, 0);
    zoo_set_context(zhandle_, &sem);

    sem_wait(&sem);
    LOG_INFO("zookeeper init success!");
}

void ZkClient::close()
{
    if (zhandle_ != nullptr)
        zookeeper_close(zhandle_);
}

bool ZkClient::exist()
{
    return zhandle_ != nullptr;
}

// 在zkserver上根据指定的path创建znode节点
void ZkClient::create(const char *path, const char *data, int datalen, int state)
{
    char path_buffer[128];
    int path_buffer_len = sizeof(path_buffer);
    int flag = zoo_exists(zhandle_, path, 0, nullptr);

    if (flag == ZNONODE)
    {
        flag = zoo_create(zhandle_, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, path_buffer_len);
        if (flag == ZOK)
            LOG_INFO("znode create success... path[%s]", path);
        else
        {
            LOG_ERROR("flag[%d],znode create error... path[%s]", flag, path);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        LOG_INFO("znode path[%s] has been created!", path);
    }
}

static void getChildrenList(zhandle_t *zh, const char *path, void *watcherCtx)
{
    struct String_vector strings;
    int rc = zoo_wget_children(zh, path, getChildrenListwatcher, watcherCtx, &strings);
    if (rc == ZOK)
    {
        // 将子节点列表更新给watcherCtx
        WriteFstMp<std::string, struct String_vector> *mp = static_cast<WriteFstMp<std::string, struct String_vector> *>(watcherCtx);

        mp->change(std::string(path), strings);

        // FIXME:释放内存怎么解决
        // deallocate_String_vector(&strings);
    }
    else
    {
        fprintf(stderr, "Failed to get children list: [%s]\n", zerror(rc));
    }
}

static void getChildrenListwatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{
    if (type == ZOO_CHILD_EVENT)
    {
        printf("Watcher triggered for path: [%s]\n", path);

        // 重新获取子节点列表
        getChildrenList(zh, path, watcherCtx);
        WriteFstMp<std::string, struct String_vector> *mp = static_cast<WriteFstMp<std::string, struct String_vector> *>(watcherCtx);

        const auto &strings = mp->get(std::string(path));
        printf("Updated children list:\n");
        for (int i = 0; i < strings.count; ++i)
            printf("%s\n", strings.data[i]);
    }
};
void ZkClient::getChildrenList(const char *path, void *watcherCtx)
{
    ::getChildrenList(zhandle_, path, watcherCtx);
}

// 根据参数指定的znode节点路径获得znode的值
std::string ZkClient::getData(const char *path)
{
    char buf[64];
    int buflen = sizeof(buf);
    int flag = zoo_get(zhandle_, path, 0, buf, &buflen, nullptr);
    if (flag != ZOK)
    {
        LOG_ERROR("get znode error... path[%s]", path);
        return "";
    }
    return buf;
}
