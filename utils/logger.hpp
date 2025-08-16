#pragma once
#ifndef HSBA_SLICER_LOGGER_HPP
#define HSBA_SLICER_LOGGER_HPP

#include <shared_mutex>
#include <source_location>

#include <boost/log/core.hpp>

#include "base/singleton.hpp"

namespace HsBa::Slicer::Log
{
	class LoggerSingletone : public Utils::Singleton<LoggerSingletone>
    {
    public:
		friend class Utils::Singleton<LoggerSingletone>;
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
        bool use_log_file_;
        std::string log_path_;
        int log_level_;
        std::string log_datatime_format_;
    };
	inline namespace LogLiteral
	{
        class LogState 
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
        LogState operator""_log_debug(const char* message, std::size_t size);
        LogState operator""_log_info(const char* message, std::size_t size);
        LogState operator""_log_warning(const char* message, std::size_t size);
        LogState operator""_log_error(const char* message, std::size_t size);
	}// namespace HsBa::Slicer::Log::LogLiteral
}// namespace HsBa::Slicer::Log

#endif // !HSBA_SLICER_LOGGER_HPP
