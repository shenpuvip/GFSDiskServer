#ifndef SSFS_UPDOWNER_H
#define SSFS_UPDOWNER_H

#include "codec.h"
#include "msgcode.h"
#include <list>
#include <memory>
#include <string>
#include <cstdio>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>

struct TASK
{
    enum{ DOWN = 1, UP = 2};
    int type = 0;
    std::string blockhash;
    std::string localfilepath;
    std::string remotefilepath;
    std::shared_ptr<FILE> fptr;
    int over = 0;
};
typedef std::shared_ptr<TASK> TASKPtr;

class UPDOWNER
{
private:
    muduo::net::TcpClient client_;
    Codec codec_;
    TASKPtr task;

public:
    UPDOWNER(muduo::net::EventLoop* loop,
             const muduo::net::InetAddress& addr,
             const TASKPtr& task);

    void connect();
    void disconnect();
    void stop();

    const TASKPtr& getTaskPtr()
    {
        return task;
    }

    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn,
                         msgtype_t msgtype, const std::string& message,
                         muduo::Timestamp time);
    void onWriteComplete(const muduo::net::TcpConnectionPtr& conn);
};

typedef std::shared_ptr<UPDOWNER> UPDOWNERPtr;

#endif //SSFS_UPDOWNER_H
