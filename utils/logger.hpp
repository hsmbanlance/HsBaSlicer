#pragma once
#ifndef HSBA_SLICER_LOGGER_HPP
#define HSBA_SLICER_LOGGER_HPP

#include <mutex>
#include <source_location>

#include <boost/log/core.hpp>

namespace HsBa::Slicer::Log
{
    class LoggerSingletone
    {
    public:
        static LoggerSingletone& GetInstance();
        static void DeleteInstance();
        bool UseLogFile() const;
        static void Log(std::string_view message,const int log_lv, const std::source_location& location = std::source_location::current());
        static void LogDebug(std::string_view message,const std::source_location& location = std::source_location::current());
        static void LogInfo(std::string_view message,const std::source_location& location = std::source_location::current());
        static void LogWarning(std::string_view message,const std::source_location& location = std::source_location::current());
        static void LogError(std::string_view message,const std::source_location& location = std::source_location::current());

    private:
        LoggerSingletone();        
        LoggerSingletone(const LoggerSingletone&) = delete;
        LoggerSingletone& operator=(const LoggerSingletone&) = delete;
        LoggerSingletone(LoggerSingletone&&) = delete;
        LoggerSingletone& operator=(LoggerSingletone&&) = delete;
        static std::mutex mutex_;
        static LoggerSingletone* instance_;
        bool use_log_file_;
        std::string log_path_;
        int log_level_;
        std::string log_datatime_format_;
    };
}// namespace HsBa::Slicer::Log

#endif // !HSBA_SLICER_LOGGER_HPP
