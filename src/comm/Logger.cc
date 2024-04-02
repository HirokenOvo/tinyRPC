#include "Logger.h"
#include <iostream>
#include <thread>

Logger::Logger()
{
    std::thread writeLogTask([this]()
                             {
    while(true)
    {
        auto [msg, filename] = lockQe_.pop();
        filename ="./log/"+filename+ "-log.txt";
        FILE *pf = fopen(filename.c_str(), "a+");
        if (pf == nullptr)
        {
            std::cout << "Logger file: " << filename << " open error!" << std::endl;
            exit(EXIT_FAILURE);
        }

        fputs(msg.c_str(), pf);
        fclose(pf);
    } });

    writeLogTask.detach();
}

Logger &Logger::getInstance()
{
    static Logger logger;
    return logger;
}

// 设置日志级别
void Logger::setLogLevel(LogLevel level)
{
    logLevel_ = level;
}

// 写日志  [级别信息] time : msg
void Logger::log(std::string msg)
{
    Timestamp now{Timestamp::now()};
    switch (logLevel_)
    {
    case INFO:
        msg = "[INFO]" + now.toString() + " : " + msg + "\n";
        break;
    case ERROR:
        msg = "[ERROR]" + now.toString() + " : " + msg + "\n";
        break;
    case FATAL:
        msg = "[FATAL]" + now.toString() + " : " + msg + "\n";
        break;
    case DEBUG:
        msg = "[DEBUG]" + now.toString() + " : " + msg + "\n";
        break;
    default:
        break;
    }
    lockQe_.push({msg, now.getYMD()});
}
