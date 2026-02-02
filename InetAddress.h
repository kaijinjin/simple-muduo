#pragma once
#include <netinet/in.h>             // sockaddr_in
#include <string>


class InetAddress
{
public:
    InetAddress(std::string ip, uint16_t port);
    InetAddress(const sockaddr_in& addr)
        : addr_(addr) {}

    std::string toIp() const;
    std::string toIpPort() const;
    // ntohs：网络字节序转主机字节序
    uint16_t toPort() const { return ntohs(addr_.sin_port); }

    const sockaddr_in* getSockAddr() const { return &addr_; }

private:
    sockaddr_in addr_;
};