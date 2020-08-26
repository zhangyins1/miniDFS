#ifndef MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
#define MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H

#include "muduo/base/Logging.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/Endian.h"
#include "muduo/net/TcpConnection.h"

class codec : muduo::noncopyable
{
 public:
  typedef std::function<void (const muduo::net::TcpConnectionPtr&,
                                const muduo::string& message,
                                muduo::Timestamp)> StringMessageCallback;

  explicit codec(const StringMessageCallback& cb)
    : messageCallback_(cb)
  {
  }

  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp receiveTime)
  {
    while (buf->readableBytes() >= kHeaderLen) // kHeaderLen == 4
    {
      // FIXME: use Buffer::peekInt32()
      header_ head;
      memcpy(&head,buf->peek(),kHeaderLen);
      if(head.type==0)
      {
        for(int i=0;i<4;i++)
        {
          if(conn==nameserver->server->connections_[i])
          {
            nameserver->dataservers[i].bufsize=head.length;
            nameserver->count++;
          }
        }
        buf->retrieve(kHeaderLen);
        if(nameserver->count==3)
          {
            std::unique_lock<std::mutex> lk(nameserver->mtx);
            nameserver->finish=true;
            lk.unlock();
            nameserver->cv.notify_all();
          }
      }
      else if(head.type==2)
      {
        if(buf->readableBytes()>kHeaderLen)
        {
          for(int i=0;i<4;i++)
          {
            if(conn==nameserver->server->connections_[i])
            {
              nameserver->dataservers[i].bufsize=head.length;
              nameserver->count++;
            }
          }
          buf->retrieve(kHeaderLen);
          if(nameserver->count==4)
          {
            std::unique_lock<std::mutex> lk(nameserver->mtx);
            nameserver->finish=true;
            lk.unlock();
            nameserver->cv.notify_all();
          }
        }
      }
      else if(head.type==1)
      {
        if(head.length==0)
        {
          for(int i=0;i<4;i++)
          {
            if(conn==nameserver->server->connections_[i])
              nameserver->dataservers[i].bufsize=0;
          }
          buf->retrieve(kHeaderLen);
        }
        else
        {
          if(buf->readableBytes()>=kHeaderLen+head.length)
          {
            buf->retrieve(kHeaderLen);
            for(int i=0;i<4;i++)
            {
              if(conn==nameserver->server->connections_[i])
              {
                nameserver->dataservers[i].buf=new char[head.length];
                memcpy(nameserver->dataservers[i].buf,buf->peek(),head.length);
                buf->retrieve(head.length);
                nameserver->count++;
              }
              if(nameserver->count==4)
              {
                std::unique_lock<std::mutex> lk(nameserver->mtx);
                nameserver->finish=true;
                lk.unlock();
                nameserver->cv.notify_all();
              }
            }
          }
        }
      }
    }
  }

  // FIXME: TcpConnectionPtr
  void send(muduo::net::TcpConnection* conn,
            const muduo::StringPiece& message)
  {
    muduo::net::Buffer buf;
    buf.append(message.data(), message.size());
    int32_t len = static_cast<int32_t>(message.size());
    int32_t be32 = muduo::net::sockets::hostToNetwork32(len);
    buf.prepend(&be32, sizeof be32);
    conn->send(&buf);
  }

 private:
  StringMessageCallback messageCallback_;
  const static size_t kHeaderLen = sizeof(int32_t);
  NameServer* nameserver;
};

#endif  // MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H