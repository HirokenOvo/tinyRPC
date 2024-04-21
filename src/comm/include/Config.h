#pragma once

#include "uncopyable.h"
#include <memory>
#include <unordered_map>

class Config : uncopyable
{
public:
    enum cfgType
    {
        client = 0,
        server
    };
    static void loadCfgFile(int argc, char **argv, cfgType tp);

    static std::string getCfg(const std::string &key);

private:
    static std::shared_ptr<std::unordered_map<std::string, std::string>> sptrUmp_;
};