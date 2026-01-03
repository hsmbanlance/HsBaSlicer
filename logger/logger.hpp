#pragma once
#ifndef HSBA_SLICER_LOGGER_HPP
#define HSBA_SLICER_LOGGER_HPP

#include <shared_mutex>
#include <source_location>

#ifndef __ANDROID__
#include <boost/log/core.hpp>
#endif

#include "base/singleton.hpp"
#include "export.h"

namespace HsBa::Slicer::Log
{
	class HSBA_SLICER_LOG_API LoggerSingletone
    {
    public:
        bool UseLogFile() const;
        static void Log(std::string_view message,const int log_lv, const std::source_location& location = std::source_location::current());
        static void LogDebug(std::string_view message,const std::source_location& location = std::source_location::current());
        static void LogInfo(std::string_view message,const std::source_location& location = std::source_location::current());
        static void LogWarning(std::string_view message,const std::source_location& location = std::source_location::current());
        static void LogError(std::string_view message,const std::source_location& location = std::source_location::current());
        static std::shared_ptr<LoggerSingletone> GetInstance();
    private:
        struct HSBA_SLICER_LOG_API Private{};

        bool use_log_file_;
        std::string log_path_;
        int log_level_;
        std::string log_datatime_format_;
    public:
        LoggerSingletone(Private);        
        LoggerSingletone(const LoggerSingletone&) = delete;
        LoggerSingletone& operator=(const LoggerSingletone&) = delete;
        LoggerSingletone(LoggerSingletone&&) = delete;
        LoggerSingletone& operator=(LoggerSingletone&&) = delete;
        static std::shared_ptr<LoggerSingletone> instance_;
        static std::shared_mutex mutex_;
        static std::once_flag instance_flag_;
    
        static std::shared_ptr<LoggerSingletone> CreateInstance();
    };
	inline namespace LogLiterals
	{
        class HSBA_SLICER_LOG_API LogState 
        {
        public:
			LogState(const int log_lv,std::string_view message);
			~LogState();
			LogState(const LogState&) = delete;
			LogState& operator=(const LogState&) = delete;
			LogState(LogState&&) = default;
			LogState& operator=(LogState&&) = default;
			void operator()(const std::source_location& location = std::source_location::current());
		private:
			int log_lv_;
			std::string message_;
        };
        HSBA_SLICER_LOG_API LogState operator""_log_debug(const char* message, std::size_t size);
        HSBA_SLICER_LOG_API LogState operator""_log_info(const char* message, std::size_t size);
        HSBA_SLICER_LOG_API LogState operator""_log_warning(const char* message, std::size_t size);
        HSBA_SLICER_LOG_API LogState operator""_log_error(const char* message, std::size_t size);
	}// namespace HsBa::Slicer::Log::LogLiteral
}// namespace HsBa::Slicer::Log

#endif // !HSBA_SLICER_LOGGER_HPP
