#include "MasterServer.h"
#include "SFS.h"

#include <functional>
#include <string>
#include <list>

using namespace std::placeholders;

MasterServer::MasterServer(muduo::net::EventLoop *loop,
                           const muduo::net::InetAddress &listenAddr,
                           const std::string &serverName)
        :server_(loop, listenAddr, serverName),
         codec_(std::bind(&MasterServer::onMessage, this, _1, _2, _3, _4)),
         loop_(loop)
{
    server_.setConnectionCallback(
            std::bind(&MasterServer::onConnection, this, _1)
    );
    server_.setMessageCallback(
            std::bind(&Codec::onMessage, &codec_, _1, _2, _3)
    );

    //设置清理超时连接
    loop->runEvery(1.0, std::bind(&MasterServer::delIdleConnection, this));
}

void MasterServer::delIdleConnection()
{
    muduo::Timestamp now = muduo::Timestamp::now();
    for (WeakConnectionList::iterator it = connectionList_.begin();
         it != connectionList_.end();)
    {
        muduo::net::TcpConnectionPtr conn = it->lock();
        if (conn)
        {
            const CSTPtr &st = boost::any_cast<const CSTPtr &>(conn->getContext());
            CHUNKST& ckst = boost::any_cast<CHUNKST&>(st->data);
            double age = timeDifference(now, ckst.lastReceiveTime);
            if (age > ckst.idleSeconds)
            {
                if (conn->connected())
                {
                    conn->shutdown();
                    LOG_INFO << "shutting down " << conn->name();
                    conn->forceCloseWithDelay(3.5);  // > round trip of the whole Internet.
                }
            }
            else if (age < 0)
            {
                LOG_WARN << "Time jump";
                ckst.lastReceiveTime = now;
            }
            else
            {
                break;
            }
            ++it;
        }
        else
        {
            LOG_WARN << "Expired";
            it = connectionList_.erase(it);
        }
    }
}

void MasterServer::start()
{
    server_.start();
}

void MasterServer::onConnection(const muduo::net::TcpConnectionPtr &conn)
{
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    if(conn->connected())
    {
        CSTPtr st = std::make_shared<CST>();
        conn->setContext(st);
        WeakTcpConnectionPtr tmp(conn);

        //清理15秒内未认证连接
        loop_->runAfter(15.0, [tmp](){
            muduo::net::TcpConnectionPtr conn = tmp.lock();
            if(conn && conn->connected())
            {
                if(conn->getContext().empty())
                {
                    conn->shutdown();
                    conn->forceCloseWithDelay(3.5);
                    return;
                }
                const CSTPtr &st = boost::any_cast<const CSTPtr &>(conn->getContext());
                if(!(st->status & CST::AUTH))
                {
                    conn->shutdown();
                    conn->forceCloseWithDelay(3.5);
                }
            }
        });

    }
    else
    {
        //TODO 连接断开相关处理
        const CSTPtr &st = boost::any_cast<const CSTPtr &>(conn->getContext());
        if(st->status & CST::CHUNK)
        {
            CHUNKST& ckst = boost::any_cast<CHUNKST&>(st->data);
            connectionList_.erase(ckst.position);

            FS->getChunkInfoPtr(ckst.ip, ckst.port)->setStatus(0);
        }
    }
}

void MasterServer::onMessage(const muduo::net::TcpConnectionPtr &conn,
                             msgtype_t msgtype, const std::string& message,
                             muduo::Timestamp time)
{
    const CSTPtr &st = boost::any_cast<const CSTPtr &>(conn->getContext());
    if(msgtype != AUTH_REQ && !(st->status & CST::AUTH))
    {
        //拒绝服务 断开链接
        LOG_INFO << "NO AUTH SHUTDOWN";
        conn->shutdown();
        conn->forceCloseWithDelay(3.5);
        return;
    }

    if(st->status & CST::CHUNK)
    {
        CHUNKST& ckst = boost::any_cast<CHUNKST&>(st->data);
        ckst.lastReceiveTime = muduo::Timestamp::now();
        connectionList_.splice(connectionList_.end(), connectionList_, ckst.position);
    }

    switch (msgtype)
    {
        case AUTH_REQ:
        {
            if(st->status & CST::AUTH)
            {
                LOG_INFO << "REAUTH";
            }

            LOG_INFO << conn->peerAddress().toIpPort() << " AUTH";
            st->status |= CST::AUTH;
            Json::Value msg;
            Json::Reader rd;
            rd.parse(message, msg);
            if(msg.isMember("type") && msg["type"] == "chunkserver")
            {
                st->status |= CST::CHUNK;
                std::string chunkIp = msg["chunkip"].asString();
                uint16_t chunkPort = msg["chunkport"].asUInt();

                CHUNKST ckst = CHUNKST(chunkIp, chunkPort, 15);
                ckst.lastReceiveTime = muduo::Timestamp::now();
                connectionList_.push_back(conn);
                ckst.position = --connectionList_.end();
                st->data = ckst;

                FS->getChunkInfoPtr(chunkIp, chunkPort)->setStatus(1);

                LOG_INFO << "AUTH CHUNK";
            }
            else
            {
                st->status |= CST::CLIENT;
                LOG_INFO << "AUTH CLIENT";
            }
        }
        break;
        case CHUNKLIST_REQ:
        {
            LOG_INFO << "REQ CHUNK_LIST";
            std::string resmsg = FS->getChunkList();
            codec_.send(conn, CHUNKLIST_RES, resmsg);
        }
        break;
        case FILELIST_REQ:
        {
            LOG_INFO << "REQ FILELIST";
            std::string resmsg = FS->getFileList();

            codec_.send(conn, FILELIST_RES, resmsg);
        }
        break;
        case FILEINFO_REQ:
        {
            LOG_INFO << "REQ FILEINFO";
            Json::Value msg;
            Json::Reader rd;
            rd.parse(message, msg);
            std::string filename = msg["filename"].asString();
            if(FS->isExistFileName(filename))
            {
                std::string resmsg = FS->getFileInfo(filename);
                codec_.send(conn, FILEINFO_RES, resmsg);
            }
            else
            {
                codec_.send(conn, FILEINFO_RES, R"!({"error": "file not exists"})!");
            }
        }
        break;
        case UPFILE_REQ:
        {
            LOG_INFO << "REQ UPFILE";
            Json::Value msg;
            Json::Reader rd;
            rd.parse(message, msg);
            std::string filename = msg["filename"].asString();
            std::string filehash = msg["filehash"].asString();
            if(FS->isExistFileName(filename))
            {
                Json::Value res;
                res = msg;
                res["error"] = "filename exists";
                Json::FastWriter fw;
                codec_.send(conn, UPFILE_RES, fw.write(res));
            }
            else if(FS->isExistFileHash(filehash))
            {
                Json::FastWriter fw;
                Json::Value res;
                res = msg;
                res["fastpass"] = 1;
                FS->addFile(filename, 0, filehash);
                codec_.send(conn, UPFILE_RES, fw.write(res));
            }
            else
            {
                Json::FastWriter fw;
                Json::Value res;
                res = msg;
                res["fastpass"] = 0;
                res["blocksize"] = Json::Int64 (64*1024*1024);
                codec_.send(conn, UPFILE_RES, fw.write(res));
            }
        }
        break;
        case UPFILE_BLOCK_REQ:
        {
            LOG_INFO << "REQ UPFILE_BLOCK";
            Json::Value msg;
            Json::Reader rd;
            rd.parse(message, msg);
            std::string filename = msg["filename"].asString();
            std::string filehash = msg["filehash"].asString();
            //Json::Value blocks = msg["blocks"];
            Json::Value res;
            Json::FastWriter fw;
            res = msg;
            for(auto& block : res["blocks"])
            {
                if(!FS->isExistBlockHash(block["blockhash"].asString()))
                {
                    ChunkInfoPtr ckptr = FS->getNextChunkInfoPtr();
                    if(!ckptr){ res["error"] = "now no chunkserver"; break;}
                    block["chunkip"] = ckptr->getIp();
                    block["chunkport"] = ckptr->getPort();
                }
            }
            codec_.send(conn, UPFILE_BLOCK_RES, fw.write(res));
        }
        break;
        case ADDFILE_REQ:
        {
            LOG_INFO << "REQ ADD FILE";
            Json::Value msg;
            Json::Reader rd;
            rd.parse(message, msg);
            std::string filename = msg["filename"].asString();
            std::string filehash = msg["filehash"].asString();
            int64_t filesize = msg["filesize"].asInt64();
            auto finfop = FS->addFile(filename, filesize, filehash);
            for(auto &block : msg["blocks"])
            {
                finfop->addBlock(block["blockhash"].asString());
            }
            Json::FastWriter fw;
            Json::Value res;
            res = msg;
            codec_.send(conn, ADDFILE_RES, fw.write(res));
        }
        break;
        case RMFILE_REQ:
        {
            LOG_INFO << "REQ RM FILE";
            Json::Reader rd;
            Json::FastWriter fw;
            Json::Value msg, res;
            rd.parse(message, msg);
            if(msg.isMember("filename") && msg["filename"].isString())
            {
                std::string filename = msg["filename"].asString();
                if(FS->rmFile(filename))
                    res["ok"] = 1;
                else
                    res["error"] = "file not exists";
                codec_.send(conn, RMFILE_RES, fw.write(res));
            }
        }
        break;
        case MVFILE_REQ:
        {
            LOG_INFO << "REQ MV FILE";
            Json::Reader rd;
            Json::FastWriter fw;
            Json::Value msg, res;
            rd.parse(message, msg);
            std::string oldfilename = "", newfilename="";
            if(msg.isMember("oldfilename") && msg["oldfilename"].isString())
                oldfilename = msg["oldfilename"].asString();
            if(msg.isMember("newfilename") && msg["newfilename"].isString())
                newfilename = msg["newfilename"].asString();
            if(FS->mvFile(oldfilename, newfilename))
                res["ok"] = 1;
            else
                res["error"] = "mvfile error";
            codec_.send(conn, MVFILE_RES, fw.write(res));
        }
        break;
        case ADDBLOCK_REQ:
        {
            LOG_INFO << "REQ ADD BLOCK";
            Json::Value msg;
            Json::Reader rd;
            rd.parse(message, msg);
            std::string blockhash = msg["blockhash"].asString();
            int64_t blocksize = msg["blocksize"].asInt64();
            FS->addBlock(blockhash, blocksize);
            CHUNKST& ckst = boost::any_cast<CHUNKST&>(st->data);
            FS->getChunkInfoPtr(ckst.ip, ckst.port)->addBlock(blockhash);
        }
        break;
        case KEEPALIVE_CK:
        {
            //LOG_INFO << "KEEPALIVE";
            //TODO 处理chunk心跳包
            //更新超时时间上面已处理

        }
        break;
    }
}

