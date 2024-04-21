#include "Config.h"
#include "Logger.h"
#include "tinyxml.h"

#define READ_XML_NODE(name, parent)                               \
    TiXmlElement *name##_node = parent->FirstChildElement(#name); \
    if (!name##_node)                                             \
    {                                                             \
        LOG_ERROR("Failed to read node[%s]", #name);              \
        exit(EXIT_FAILURE);                                       \
    }

#define READ_STR_FROM_XML_NODE(name, parent)                      \
    TiXmlElement *name##_node = parent->FirstChildElement(#name); \
    if (!name##_node || !name##_node->GetText())                  \
    {                                                             \
        LOG_ERROR("Failed to read config file[%s]", #name);       \
        exit(EXIT_FAILURE);                                       \
    }                                                             \
    std::string name##_str = std::string(name##_node->GetText());

std::shared_ptr<std::unordered_map<std::string, std::string>> Config::sptrUmp_{nullptr};

void Config::loadCfgFile(int argc, char **argv, cfgType tp)
{
    if (argc != 2)
    {
        std::cout << "format: path/to/executable path/to/config" << std::endl;
        exit(EXIT_FAILURE);
    }
    const char *xmlfile = argv[1];
    TiXmlDocument xml_doc;
    bool ok = xml_doc.LoadFile(xmlfile);
    if (!ok)
    {
        LOG_ERROR("Failed to load config file:[%s]! error info:[%s]", xmlfile, xml_doc.ErrorDesc());
        exit(EXIT_FAILURE);
    }
    Config::sptrUmp_ = std::make_shared<std::unordered_map<std::string, std::string>>();

    READ_XML_NODE(root, (&xml_doc));

    READ_XML_NODE(zookeeper, root_node);
    READ_STR_FROM_XML_NODE(zk_ip, zookeeper_node);
    READ_STR_FROM_XML_NODE(zk_port, zookeeper_node);
    (*sptrUmp_)["zk_ip"] = zk_ip_str;
    (*sptrUmp_)["zk_port"] = zk_port_str;

    if (tp == cfgType::server)
    {
        READ_XML_NODE(rpcServer, root_node);
        READ_STR_FROM_XML_NODE(serv_ip, rpcServer_node);
        READ_STR_FROM_XML_NODE(serv_port, rpcServer_node);
        (*sptrUmp_)["serv_ip"] = serv_ip_str;
        (*sptrUmp_)["serv_port"] = serv_port_str;
    }
}

std::string Config::getCfg(const std::string &key)
{
    if (sptrUmp_ == NULL)
    {
        LOG_ERROR("load config file first!");
        exit(EXIT_FAILURE);
    }
    if (!sptrUmp_->count(key))
    {
        LOG_ERROR("config can't find [%s]", key.c_str());
        exit(EXIT_FAILURE);
    }
    return (*sptrUmp_)[key];
}