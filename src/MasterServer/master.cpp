#include "MasterServer.h"
#include "SFS.h"

#include <SimpleIni.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

int main(int argc, char** argv)
{
    std::string listenHost = "0.0.0.0";
    uint16_t listenPort = 61481;
    std::string masterName("masterserver");

    if(argc==2 && argv[1][0] == '-')
    {
        CSimpleIniA ini;
        std::string filename(&argv[1][1]);
        FILE* inifp = fopen(filename.c_str(), "r");
        if(!inifp)
        {
            printf("Can't open config file!\n");
            return 0;
        }
        ini.LoadFile(inifp);
        listenHost = ini.GetValue("master", "host", "127.0.0.1");
        listenPort = std::stoi(ini.GetValue("master", "port", "61481"));
        masterName = ini.GetValue("master", "name", "masterserver");
    }
    else
    {
        if(argc == 2)
        {
            listenPort = std::stoi(argv[1]);
        }
        else if(argc == 3)
        {
            listenHost = argv[1];
            listenPort = std::stoi(argv[2]);
        }
    }

    muduo::net::EventLoop loop;
    muduo::net::InetAddress listenAddr(listenHost, listenPort);
    MasterServer masterServer(&loop, listenAddr, masterName);
    masterServer.start();
    loop.loop();
    return 0;
}