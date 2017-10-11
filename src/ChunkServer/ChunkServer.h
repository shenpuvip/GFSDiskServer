#ifndef SSFS_CHUNKSERVER_H
#define SSFS_CHUNKSERVER_H

#include <muduo/base/Logging.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>

#include <list>
#include <string>
#include "msgcode.h"
#include "codec.h"

#include "UPDOWNER.h"

class ChunkServer
{
private:
    struct CST;
    typedef std::shared_ptr<CST> CSTPtr;

    const std::string fileDir_; //存储根目录

    muduo::net::EventLoop *loop_;

    muduo::net::TcpServer server_;
    Codec serverCodec_;
    muduo::net::InetAddress serverAddr_;

    muduo::net::InetAddress masterAddr_;
    muduo::net::TcpClient client_;
    Codec clientCodec_;
    muduo::net::TcpConnectionPtr clientConn_;
    muduo::net::TimerId keepalive_TimerId_;

    std::list<UPDOWNERPtr> uploaderList_;
    muduo::net::TimerId delUPDOWNER_TimerId_;

public:
    explicit ChunkServer(muduo::net::EventLoop* loop,
                         const muduo::net::InetAddress& listenAddr,
                         const std::string& chunkName,
                         const std::string& fileDir,
                         const muduo::net::InetAddress& masterAddr);

    void start();

private:
    void onServerConnection(const muduo::net::TcpConnectionPtr& conn);
    void onServerMessage(const muduo::net::TcpConnectionPtr& conn,
                         msgtype_t msgtype, const std::string& message,
                         muduo::Timestamp time);
    void onServerWriteComplete(const muduo::net::TcpConnectionPtr& conn);

    //TODO 封装发送给master的函数
    void onClientConnection(const muduo::net::TcpConnectionPtr& conn);
    void onClientMessage(const muduo::net::TcpConnectionPtr& conn,
                         msgtype_t msgtype, const std::string& message,
                         muduo::Timestamp time);
    //void onClientWriteComplete(const muduo::net::TcpConnectionPtr& conn);

    void runClientKeepAlive();
};




#endif //SSFS_CHUNKSERVER_H
