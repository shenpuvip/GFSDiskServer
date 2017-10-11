#include "ChunkServer.h"

#include <functional>
#include <memory>
using namespace std::placeholders;

#include <sys/stat.h>
#include <sys/statfs.h>

#include <json/json.h>

ChunkServer::ChunkServer(muduo::net::EventLoop *loop,
                         const muduo::net::InetAddress &listenAddr,
                         const std::string &chunkName,
                         const std::string& fileDir,
                         const muduo::net::InetAddress& masterAddr)
        :loop_(loop),
         server_(loop, listenAddr, chunkName),
         serverCodec_(std::bind(&ChunkServer::onServerMessage, this, _1, _2, _3, _4)),
         serverAddr_(listenAddr),
         fileDir_(fileDir),
         masterAddr_(masterAddr),
         client_(loop, masterAddr, chunkName),
         clientCodec_(std::bind(&ChunkServer::onClientMessage, this, _1, _2, _3, _4))
{
    //chunkserver set callback
    server_.setConnectionCallback(
            std::bind(&ChunkServer::onServerConnection, this, _1)
    );
    server_.setMessageCallback(
            std::bind(&Codec::onMessage, &serverCodec_, _1, _2, _3)
    );
    server_.setWriteCompleteCallback(
            std::bind(&ChunkServer::onServerWriteComplete, this, _1)
    );

    //chunkclient set callback
    client_.setConnectionCallback(
            std::bind(&ChunkServer::onClientConnection, this, _1)
    );
    client_.setMessageCallback(
            std::bind(&Codec::onMessage, &clientCodec_, _1, _2, _3)
    );
    client_.enableRetry();
}

void ChunkServer::start()
{
    server_.start();
    client_.connect();
}


typedef std::shared_ptr<FILE> FilePtr;
struct ChunkServer::CST
{
    CST()
    {
        status = 0;
        upfptr = nullptr;
        downfptr = nullptr;
    }
    enum { DOWN = 1, UP = 2};
    int status;
    FilePtr upfptr;
    std::string upfilename;
    int64_t upfilesize;
    FilePtr downfptr;
};

void ChunkServer::onServerConnection(const muduo::net::TcpConnectionPtr &conn)
{
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    if (conn->connected())
    {
        CSTPtr ctx = std::make_shared<CST>(CST());
        conn->setContext(ctx);
    }
    else
    {
        ;
    }
}

void ChunkServer::onServerMessage(const muduo::net::TcpConnectionPtr &conn, msgtype_t msgtype, const std::string &message,
                                  muduo::Timestamp time)
{
    const CSTPtr &st = boost::any_cast<const CSTPtr &>(conn->getContext());
    switch (msgtype)
    {
        case UPBLOCK_DATA:
        {
            LOG_INFO << "UPBLOCK_DATA";
            if (st->status & CST::UP)
            {
                if (st->upfptr.get())
                {
                    std::fwrite(message.data(), message.size(), 1, st->upfptr.get());
                }
                else
                {
                    LOG_WARN << "上传状态 文件指针为空";
                }
            }
            else
            {
                //serverCodec_.send(conn, UPBLOCK_STOP);
                LOG_WARN << "非上传状态 收到UPBLOCK_DATA";
            }
        }
            break;
        case UPBLOCK_OVER:
        {
            LOG_INFO << "UPBLOCK OVER";
            if (st->status & CST::UP)
            {
                st->status &= ~CST::UP;
                st->upfptr.reset();
                Json::Value req;
                Json::FastWriter fw;
                req["blockhash"] = st->upfilename;
                req["blocksize"] = Json::Int64(st->upfilesize);

                int ret = rename((fileDir_ + "/tmp_" + st->upfilename).data(), (fileDir_ + st->upfilename).data());
                remove((fileDir_ + "/tmp_" + st->upfilename).data());

                if(ret == 0)
                {
                    //向客户端回复上传完成
                    serverCodec_.send(conn, UPBLOCK_OVER, fw.write(req));

                    //FIXME clientConn_ 可能与master连接断开
                    clientCodec_.send(clientConn_, ADDBLOCK_REQ, fw.write(req));
                }
                else
                {
                    req["error"] = "upblock failed";
                    serverCodec_.send(conn, UPBLOCK_OVER, fw.write(req));
                }
            }
            else
            {
                LOG_WARN << "非上传状态 收到UPBLOCK_OVER";
            }
        }
            break;
        case UPBLOCK_STOP:
        {
            if (st->status & CST::UP)
            {
                st->status &= ~CST::UP;
                st->upfptr.reset();
            }
            else
            {
                LOG_WARN << "非上传状态 收到UPBLOCK_STOP";
            }
        }
            break;
        case UPBLOCK_REQ:
        {
            LOG_INFO << "REQ UPBLOCK";
            if(st->status & CST::UP)
            {
                serverCodec_.send(conn, UPBLOCK_RES, R"msg({"error":"now have upload file"})msg");
                break;
            }

            Json::Reader reader;
            Json::FastWriter writer;
            Json::Value msg;
            Json::Value res;
            if(reader.parse(message, msg))
            {
                std::string filename = fileDir_ + "/tmp_";
                int flag = 0;
                res = msg;
                try
                {
                    filename += msg["blockhash"].asString();
                }
                catch (...)
                {
                    LOG_ERROR << "REQ upload msg error:" << message;
                    flag = 1;
                    res["error"] = "json msg error";
                }

                if(flag == 0)
                {
                    FILE* fp = fopen(filename.data(), "wb");
                    st->upfptr.reset(fp, std::fclose);
                    st->upfilename = msg["blockhash"].asString();
                    st->upfilesize = msg["blocksize"].asInt64();
                    st->status |= CST::UP;
                }

                serverCodec_.send(conn, UPBLOCK_RES, writer.write(res));
            }
            else
            {
                LOG_WARN << "bad req json UPLOADFILE:" << message;
            }
        }
            break;
        case DOWNBLOCK_REQ:
        {
            LOG_INFO << "REQ DOWNBLOCK";
            if(st->status & CST::DOWN)
            {
                //serverCodec_.send(conn, DOWNBLOCK_RES, R"msg({"error":"now have download file"})msg");
                break;
            }

            Json::Reader reader;
            Json::FastWriter writer;
            Json::Value msg;
            Json::Value res;
            if(reader.parse(message, msg))
            {
                std::string filename = fileDir_;
                int flag = 0;
                res = msg;
                try
                {
                    filename += msg["blockhash"].asString();
                }
                catch (...)
                {
                    LOG_ERROR << "REQ download msg error:" << message;
                    flag = 1;
                    res["error"] = "json msg error";
                }

                if(flag == 0)
                {
                    struct stat stbuf;
                    FILE* fp = nullptr;
                    if(::stat(filename.data(), &stbuf) == 0 && S_ISREG(stbuf.st_mode)
                       && (fp = fopen(filename.data(), "rb")) != nullptr )
                    {
                        LOG_INFO << "blockname:" << filename;
                        res["blocksize"] = Json::Int64(stbuf.st_size);
                        st->downfptr.reset(fp, std::fclose);
                        st->status |= CST::DOWN;
                    }
                    else
                    {
                        res["blocksize"] = Json::Int64(-1);
                        res["error"] = "file does exists";
                    }
                }

                serverCodec_.send(conn, DOWNBLOCK_RES, writer.write(res));
            }
            else
            {
                LOG_WARN << "bad req json DOWNLOADFILE: " << message;
            }
        }
            break;
        case DOWNBLOCK_STOP:
        {
            if(st->status & CST::DOWN)
            {
                st->status &= ~CST::DOWN;
                st->downfptr.reset();
            }
            else
            {
                LOG_WARN << "error msg";
            }
        }
            break;
        default:
        {
            LOG_WARN << "BAD MSGTYPE: " << msgtype << " " << message;
        }
    }
}

void ChunkServer::onServerWriteComplete(const muduo::net::TcpConnectionPtr &conn)
{
    const CSTPtr &st = boost::any_cast<const CSTPtr &>(conn->getContext());
    if (st->status & CST::DOWN)
    {
        if (st->downfptr.get())
        {
            LOG_INFO << "DOWNBLOCKing";
            char buf[1024 * 1024];
            int len = std::fread(buf, 1, sizeof(buf), st->downfptr.get());
            if (len > 0)
            {
                serverCodec_.send(conn, DOWNBLOCK_DATA, buf, len);
            }
            else
            {
                st->status &= ~CST::DOWN;
                st->downfptr.reset();
                serverCodec_.send(conn, DOWNBLOCK_OVER);
                LOG_INFO << "RES DOWNOVER";
            }
        }
    }
}


void ChunkServer::onClientConnection(const muduo::net::TcpConnectionPtr &conn)
{
    LOG_INFO << "CONNECT TO MASTER SERVER: " << (conn->connected() ? "UP" : "DOWN");

    if(conn->connected())
    {
        keepalive_TimerId_ = loop_->runEvery(5.0, std::bind(&ChunkServer::runClientKeepAlive, this));

        //TODO 发送认证信息
        Json::FastWriter fw;
        Json::Value req;
        req["type"] = "chunkserver";
        req["chunkip"] = serverAddr_.toIp();
        req["chunkport"] = serverAddr_.toPort();
        clientCodec_.send(conn, AUTH_REQ, fw.write(req));
        clientConn_ = conn;

        delUPDOWNER_TimerId_ = loop_->runEvery(15, [this]()
        {
            for(auto it = uploaderList_.begin(); it != uploaderList_.end();)
            {
                if((*it)->getTaskPtr()->over != 0)
                {
                    //TODO 处理任务失败情况
                    it = uploaderList_.erase(it);
                }
                else
                    ++it;
            }
        });
    }
    else
    {
        loop_->cancel(keepalive_TimerId_);
        keepalive_TimerId_ = muduo::net::TimerId();
        clientConn_.reset();

        loop_->cancel(delUPDOWNER_TimerId_);
    }
}

void ChunkServer::onClientMessage(const muduo::net::TcpConnectionPtr &conn, msgtype_t msgtype, const std::string &message,
                                  muduo::Timestamp time)
{
    //TODO 处理master下发信息
    switch (msgtype)
    {
        case BACKUP_BLOCK_REQ:
        {
            ;
        }
        break;
    }
}

void ChunkServer::runClientKeepAlive()
{
    if(client_.connection() && client_.connection()->connected())
    {
        struct statfs diskInfo;
        statfs(fileDir_.data(), &diskInfo);
        int64_t freeSize = diskInfo.f_blocks * diskInfo.f_bsize;
        Json::FastWriter fw;
        Json::Value req;
        req["freesize"] = Json::Int64 (freeSize);
        clientCodec_.send(client_.connection(), KEEPALIVE_CK, fw.write(req));
    }
}
