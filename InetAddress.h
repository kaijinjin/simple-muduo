#pragma once
#include <netinet/in.h>             // sockaddr_in
#include <string>


class InetAddress
{
public:
    InetAddress(std::string ip = "127.0.0.1", uint16_t port = 0);
    InetAddress(const sockaddr_in& addr)
        : addr_(addr) {}

    std::string toIp() const;
    std::string toIpPort() const;
    // ntohs：网络字节序转主机字节序
    uint16_t toPort() const { return ntohs(addr_.sin_port); }

    const sockaddr_in* getSockAddr() const { return &addr_; }
    void setSockAddr(const sockaddr_in& addr) { addr_ = addr; }

private:
    sockaddr_in addr_;
};