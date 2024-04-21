#include "tinyxml.h"
#include <iostream>

int main(int argc, char **argv)
{
    std::cout << argc << std::endl;
    const char *xmlfile = argv[1];

    TiXmlDocument xml_doc;
    bool ok = xml_doc.LoadFile(xmlfile);
    std::cout << xmlfile << std::endl;

    if (!ok)
    {
        printf("Failed to load config file:[%s]! error info:[%s]", xmlfile, xml_doc.ErrorDesc());
        exit(EXIT_FAILURE);
    }
}