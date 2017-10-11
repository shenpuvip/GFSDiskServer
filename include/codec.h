#ifndef SSFS_CODEC_H
#define SSFS_CODEC_H


#include <cstdint>
#include <muduo/base/Logging.h>
#include <muduo/net/TcpConnection.h>
#include "msgcode.h"

class Codec
{
private:
    typedef std::function<void (const muduo::net::TcpConnectionPtr&,
                                msgtype_t msgtype,
                                const muduo::string& message,
                                muduo::Timestamp)> StringMessageCallback;
    StringMessageCallback messageCallback_;

public:

    explicit Codec(const StringMessageCallback& cb):messageCallback_(cb)
    {
        ;
    }

    void onMessage(const muduo::net::TcpConnectionPtr& conn,
                   muduo::net::Buffer* buf, muduo::Timestamp time)
    {
        while(buf->readableBytes() >= HLEN)
        {
            msgh_t len = buf->peekInt32();
            if(len < MINHLEN || len > MAXHLEN)
            {
                conn->shutdown();
                break;
            }
            else if(buf->readableBytes() >= HLEN + len)
            {
                buf->retrieve(HLEN);
                msgflag_t flag = buf->readInt16();
                if(flag != FLAG)
                {
                    conn->shutdown();
                    break;
                }
                msgtype_t msgtype = buf->readInt16();
                len -= FLEN + MSGTYPELEN;
                muduo::string message(buf->peek(), len);
                messageCallback_(conn, msgtype, message, time);

                buf->retrieve(len);
            }
            else
            {
                break;
            }
        }
    }

    void send(const muduo::net::TcpConnectionPtr& conn,
              msgtype_t msgtype)
    {
        muduo::net::Buffer buf;
        buf.appendInt16(FLAG);
        buf.appendInt16(msgtype);
        buf.prependInt32(FLEN+MSGTYPELEN);
        conn->send(&buf);
    }

    void send(const muduo::net::TcpConnectionPtr& conn,
              const muduo::StringPiece& message)
    {
        muduo::net::Buffer buf;
        buf.appendInt16(FLAG);
        buf.append(message.data(), message.size());
        int32_t len = static_cast<int32_t>(message.size());
        buf.prependInt32(len+FLEN);
        conn->send(&buf);
    }

    void send(const muduo::net::TcpConnectionPtr& conn,
              const char* message, int32_t len)
    {
        muduo::net::Buffer buf;
        buf.appendInt16(FLAG);
        buf.append(message, len);
        buf.prependInt32(len+FLEN);
        conn->send(&buf);
    }

    void send(const muduo::net::TcpConnectionPtr& conn,
              msgtype_t msgtype,
              const muduo::StringPiece& message)
    {
        muduo::net::Buffer buf;
        buf.appendInt16(FLAG);
        buf.appendInt16(msgtype);
        buf.append(message.data(), message.size());
        int32_t len = static_cast<int32_t>(message.size());
        buf.prependInt32(len+FLEN+MSGTYPELEN);
        conn->send(&buf);
    }

    void send(const muduo::net::TcpConnectionPtr& conn,
              msgtype_t msgtype,
              const char* message, int32_t len)
    {
        muduo::net::Buffer buf;
        buf.appendInt16(FLAG);
        buf.appendInt16(msgtype);
        buf.append(message, len);
        buf.prependInt32(len+FLEN+MSGTYPELEN);
        conn->send(&buf);
    }
};


#endif //SSFS_CODEC_H
