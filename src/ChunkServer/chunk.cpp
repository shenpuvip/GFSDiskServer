#include "ChunkServer.h"
#include <string>

#include <SimpleIni.h>

int main(int argc, char** argv)
{
    std::string listenHost = "127.0.0.1";
    uint16_t listenPort = 61485;
    std::string chunkName("chunkserver");
    std::string fileDir = "./";
    std::string masterHost = "127.0.0.1";
    uint16_t masterPort = 61481;

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
        listenHost = ini.GetValue("chunk", "host", "127.0.0.1");
        listenPort = std::stoi(ini.GetValue("chunk", "port", "61485"));
        chunkName = ini.GetValue("chunk", "name", "chunkserver");
        fileDir   = ini.GetValue("chunk", "dir", "./");

        masterHost = ini.GetValue("master", "host", "127.0.0.1");
        masterPort = std::stoi(ini.GetValue("master", "port", "61481"));
    }
    else
    {
        if(argc == 2)
        {
            listenPort = std::stoi(argv[1]);
        }
        else if(argc >= 3)
        {
            listenHost = argv[1];
            listenPort = std::stoi(argv[2]);
            if(argc > 3)
                fileDir = argv[3];
        }
    }

    muduo::net::EventLoop loop;
    muduo::net::InetAddress listenAddr(listenHost, listenPort);
    muduo::net::InetAddress masterAddr(masterHost, masterPort);
    ChunkServer chunkServer(&loop, listenAddr, chunkName, fileDir, masterAddr);
    chunkServer.start();
    loop.loop();

    return 0;
}