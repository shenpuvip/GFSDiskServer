#ifndef SSFS_MASTERSERVER_H
#define SSFS_MASTERSERVER_H

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpConnection.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/any.hpp>

#include <string>
#include <list>
#include "msgcode.h"
#include "codec.h"

class MasterServer
{
private:
    muduo::net::EventLoop *loop_;
    muduo::net::TcpServer server_;
    Codec codec_;

    typedef boost::weak_ptr<muduo::net::TcpConnection> WeakTcpConnectionPtr;
    typedef std::list<WeakTcpConnectionPtr> WeakConnectionList;
    WeakConnectionList connectionList_;
    void delIdleConnection();

    struct CHUNKST
    {

        CHUNKST(const std::string& ip, uint16_t port, int idleSeconds = 15)
                :ip(ip), port(port), idleSeconds(idleSeconds)
        {
            ;
        }

        std::string ip;
        uint16_t port;

        int idleSeconds;
        muduo::Timestamp lastReceiveTime;
        WeakConnectionList::iterator position;
    };

    struct CLIENTST
    {
        std::string username;
    };

    struct CST
    {
        CST()
        {
            status = 0;
        }
        enum { AUTH = 1, CHUNK = 2, CLIENT = 4 };
        int status;
        boost::any data;
    };
    typedef std::shared_ptr<CST> CSTPtr;


public:
    MasterServer(muduo::net::EventLoop* loop,
                 const muduo::net::InetAddress& listenAddr,
                 const std::string& serverName);

    void start();

private:
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn,
                   msgtype_t msgtype, const std::string& message,
                   muduo::Timestamp time);

    void backupBlock();
};



#endif //SSFS_MASTERSERVER_H
