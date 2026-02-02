#pragma once
#include <cstdint>              // int64_t
#include <string>
#include <ctime>                // time、localtime


class Timestamp
{
public:
    // 默认构造，当Timestamp默认构造时对secondSinceEpoch_进行初始化，避免垃圾值
    Timestamp()
        : secondSinceEpoch_(0) {}
    explicit Timestamp(uint64_t secondSinceEpoch)
        : secondSinceEpoch_(secondSinceEpoch) {}
    // 提供获取当前时间的方法，设置为静态方便匿名使用
    static Timestamp now() { return Timestamp(time(nullptr)); }
    // 输出字符串xxxx-xx-xx xx:xx:xx格式的时间
    std::string toString() const;

private:
    // 表示从1970年1月1日 00:00:00 开始计时，精确到秒
    int64_t secondSinceEpoch_;
};