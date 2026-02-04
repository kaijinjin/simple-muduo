#include "InetAddress.h"

#include <cstring>              // memset
#include <arpa/inet.h>          // inet_addr


InetAddress::InetAddress(std::string ip, uint16_t port)
{
    // 使用memset替代bzero，bzero已经被废弃，虽然linux下的glibc依然有提供
    memset(&addr_, 0x00, sizeof(addr_));
    // AF_INET：ipv4
    addr_.sin_family = AF_INET;
    // htons：主机字节序转网络字节序
    addr_.sin_port = htons(port);
    // 使用inet_pton替代inet_addr，inet_addr已经被标记废弃
    inet_pton(AF_INET, ip.c_str(), &(addr_.sin_addr));
}

std::string InetAddress::toIp() const
{
    char buf[16] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));

    return buf;
}

std::string InetAddress::toIpPort() const
{
    // ip:port
    return (toIp() + ":" + std::to_string(toPort()));
}

