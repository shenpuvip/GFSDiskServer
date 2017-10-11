#include "UPDOWNER.h"

#include <functional>
using namespace std::placeholders;

#include <json/json.h>


UPDOWNER::UPDOWNER(muduo::net::EventLoop *loop,
                   const muduo::net::InetAddress &addr,
                   const TASKPtr& task)
        :client_(loop, addr, "UPDOWNER"),
         codec_(std::bind(&UPDOWNER::onMessage, this, _1, _2, _3, _4)),
         task(task)
{
    client_.setConnectionCallback(
            std::bind(&UPDOWNER::onConnection, this, _1)
    );

    client_.setMessageCallback(
            std::bind(&Codec::onMessage, &codec_, _1, _2, _3)
    );

    client_.setWriteCompleteCallback(
            std::bind(&UPDOWNER::onWriteComplete, this, _1)
    );

    //client_.enableRetry();
}

void UPDOWNER::connect()
{
    client_.connect();
}

void UPDOWNER::disconnect()
{
    client_.disconnect();
}

void UPDOWNER::stop()
{
    client_.stop();
}

void UPDOWNER::onConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if(conn->connected())
    {
        if(task->type == TASK::DOWN)
        {
            Json::FastWriter fw;
            Json::Value req;
            req["blockhash"] = task->blockhash;
            codec_.send(conn, DOWNBLOCK_REQ, fw.write(req));
        }
        else if(task->type == TASK::UP)
        {
            Json::FastWriter fw;
            Json::Value req;
            req["blockhash"] = task->blockhash;
            req["blocksize"] = Json::Int64 (-1);
            codec_.send(conn, DOWNBLOCK_REQ, fw.write(req));
        }
    }
}

void UPDOWNER::onMessage(const muduo::net::TcpConnectionPtr &conn, msgtype_t msgtype, const std::string &message,
                         muduo::Timestamp time)
{
    Json::Reader rd;
    Json::FastWriter fw;
    Json::Value msg;
    switch (msgtype)
    {
        case DOWNBLOCK_RES:
        {
            if(!rd.parse(message, msg) || msg.isMember("error"))
            {
                LOG_INFO << "ERROR";
                task->over = -1;
            }
            else
            {
                FILE* fp = std::fopen(task->localfilepath.data(), "wb");
                task->fptr.reset(fp, std::fclose);
            }
        }break;
        case DOWNBLOCK_DATA:
        {
            if(task->fptr.get())
            {
                std::fwrite(message.data(), message.size(), 1, task->fptr.get());
            }
        }break;
        case DOWNBLOCK_OVER:
        {
            if(task->fptr.get())
            {
                task->fptr.reset();
                conn->shutdown();
                task->over = 1;
            }
        }break;
        case DOWNBLOCK_STOP:
        {
            if(task->fptr.get())
            {
                task->fptr.reset();
                task->over = -1;
            }
        }break;
        case UPBLOCK_RES:
        {
            if(!rd.parse(message, msg) || msg.isMember("error"))
            {
                LOG_INFO << "ERROR";
                task->over = -1;
            }
            else
            {
                FILE* fp = std::fopen(task->localfilepath.data(), "wb");
                task->fptr.reset(fp, std::fclose);
                onWriteComplete(conn);
            }
        }break;
    }
}

void UPDOWNER::onWriteComplete(const muduo::net::TcpConnectionPtr &conn)
{
    if(task->type == TASK::UP && task->fptr)
    {
        char buf[1024 * 1024];
        int len = std::fread(buf, 1, sizeof(buf), task->fptr.get());
        if(len > 0)
        {
            codec_.send(conn, UPBLOCK_DATA, buf, len);
        }
        else
        {
            codec_.send(conn, UPBLOCK_OVER);
            conn->shutdown();
            task->over = 1;
        }
    }
}