#include "Logger.h"
#include "Timestamp.h"

#include <iostream>


// 打印日志 [级别信息] 时间 : msg
void Logger::log(std::string msg)
{
    switch (logLevel_)
    {
    case LogLevel::INFO:
        std::cout << "[ INFO ]";
        break;
    case LogLevel::ERROR:
        std::cout << "[ ERROR ]";
        break;
    case LogLevel::FATAL:
        std::cout << "[ FATAL ]";
        break;
    case LogLevel::DEBUG:
        std::cout << "[ DEBUG ]";
        break;
    default:
        break;
    }

    std::cout << Timestamp::now().toString() << ":" << msg << std::endl;
}
