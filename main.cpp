#include <iostream>
#include <unistd.h>
#include <memory>

#include "Timestamp.h"
#include "Logger.h"
#include "InetAddress.h"

using namespace std;



int main()
{   
    {
        cout << Timestamp::now().toString() << endl;
        cout << Timestamp().toString() << endl;
        cout << Timestamp(time(nullptr)).toString() << endl;
    }

    {
        LOG_INFO("%s%d", "测试", 123);
        // sleep(1);
        LOG_ERROR("%s%d", "测试", 123);
        // LOG_FATAL("%s%d", "测试", 123);
        LOG_DEBUG("%s%d", "测试", 123);
    }

    {
        InetAddress address("127.0.0.1", 8000);
        cout << address.toIp() << endl;
        cout << address.toPort() << endl;
        cout << address.toIpPort() << endl;
        const sockaddr_in* addr = address.getSockAddr();
        InetAddress address2(*addr);
        cout << address2.toIp() << endl;
        cout << address2.toPort() << endl;
        cout << address2.toIpPort() << endl;
    }
    cout << "test" << endl;
    {
        int* p = new int(10);
        unique_ptr<int> up(p);
        cout << *up << endl;
        int* pp = p;
        cout << *pp << endl;
    }
    return 0;
}