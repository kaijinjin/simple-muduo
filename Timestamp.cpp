#include "Timestamp.h"


std::string Timestamp::toString() const
{

    // time_t类型在ubuntu2204-64是一个long int类型而int64_t是signed long int
    time_t seconds = static_cast<time_t>(secondSinceEpoch_);

    // C++11 值初始化
    char buf[32] = {0};
    struct tm tm_time = {0};

    // localtime_r 保证线程安全
    localtime_r(&seconds, &tm_time);
    // 格式化
    snprintf(buf, sizeof(buf), "%4d/%02d/%02d %02d:%02d:%02d", 
        tm_time.tm_year + 1900, 
        tm_time.tm_mon + 1, 
        tm_time.tm_mday, 
        tm_time.tm_hour, 
        tm_time.tm_min, 
        tm_time.tm_sec
    );
    
    return buf;
}

