#pragma once

#include "Config.h"
#include "RpcController.h"
#include <zookeeper/zookeeper.h>
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

class RpcChannel : public google::protobuf::RpcChannel
{
public:
    void CallMethod(const google::protobuf::MethodDescriptor *method,
                    google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request,
                    google::protobuf::Message *response,
                    google::protobuf::Closure *done);
    void CallMethod(const google::protobuf::MethodDescriptor *method,
                    RpcController *controller,
                    const google::protobuf::Message *request,
                    google::protobuf::Message *response,
                    google::protobuf::Closure *done,
                    int cnt);
    // FIXME : 添加cache
    // private:
    // static std::map<std::string, struct String_vector> host_list_cache_;
};
