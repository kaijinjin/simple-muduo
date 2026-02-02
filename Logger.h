#pragma once
#include <cstdint>              // uint8_t
#include <string>

#include "noncopyable.h"
#include "nonmoveable.h"


// 提供宏的形式来使用日志
#define LOG_INFO(logmsgFormat, ...) \
    do \
    { \
        Logger& logger = Logger::getInstance(); \
        logger.setLogLevel(LogLevel::INFO); \
        char buf[1024] = {0}; \
        snprintf(buf, sizeof(buf), logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \         
    } while (0)

#define LOG_ERROR(logmsgFormat, ...) \
    do \
    { \
        Logger& logger = Logger::getInstance(); \
        logger.setLogLevel(LogLevel::ERROR); \
        char buf[1024] = {0}; \
        snprintf(buf, sizeof(buf), logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \         
    } while (0)

#define LOG_FATAL(logmsgFormat, ...) \
    do \
    { \
        Logger& logger = Logger::getInstance(); \
        logger.setLogLevel(LogLevel::FATAL); \
        char buf[1024] = {0}; \
        snprintf(buf, sizeof(buf), logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \      
        exit(-1); \   
    } while (0)

#define LOG_DEBUG(logmsgFormat, ...) \
    do \
    { \
        Logger& logger = Logger::getInstance(); \
        logger.setLogLevel(LogLevel::DEBUG); \
        char buf[1024] = {0}; \
        snprintf(buf, sizeof(buf), logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \         
    } while (0)


// C++11的枚举类使用
enum class LogLevel : uint8_t
{
    INFO,       // 普通信息
    ERROR,       // 错误信息
    FATAL,       // 程序崩溃信息
    DEBUG        // 调试信息
};


class Logger : private noncopyable, private nonmoveable
{
public:
    // 获取单例对象-如果编译成动态库，单例在某些情况下会失效，不能将实现放在头文件要考虑其他的设计或者实现
    static Logger& getInstance()
    {
        // 1、懒汉式单例模式-C++11局部静态变量只初始化一次-保证线程安全
        // 2、类内定义默认是inline，所有使用到inline函数的地方都是使用的同一份inline函数，而不是相互独立
        static Logger logger;
        return logger;
    }
    // 设置日志级别-简单实现放在头文件中可能会内联-优化性能
    void setLogLevel(LogLevel level) { logLevel_ = level; }
    // 打印日志
    void log(std::string msg);

private:
    // 私有化构造方法-单例模式
    Logger() = default;
    LogLevel logLevel_;
};