#include "logger.hpp"
#include <cstddef>
#include <filesystem>

#ifndef __ANDROID__
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#else
#include <android/log.h>
#endif // !__ANDROID__

#include "fileoperator/rw_ptree.hpp"

#include "logger.hpp"

namespace HsBa::Slicer::Log
{
    // Define constants for logger configuration
    constexpr size_t SIZE_1KB = 1024;
    constexpr size_t SIZE_1MB = SIZE_1KB * 1024;
    constexpr size_t SIZE_50MB = 50 * SIZE_1MB;  // Used for log rotation size
	namespace
	{
		// internal helper: format source location
		std::string GetSourceLocation(const std::source_location& location)
		{
			return "[" + std::string{ location.file_name() } + ":" + std::to_string(location.line()) + "] " + location.function_name() + ": ";
		}

#ifndef __ANDROID__
		static boost::log::trivial::severity_level GetLogLevel(int log_level)
		{
			switch (log_level)
			{
			case 0: return boost::log::trivial::trace;
			case 1: return boost::log::trivial::debug;
			case 2: return boost::log::trivial::info;
			case 3: return boost::log::trivial::warning;
			case 4: return boost::log::trivial::error;
			case 5: return boost::log::trivial::fatal;
			default:
#if _DEBUG
				return boost::log::trivial::debug;
#else
				return boost::log::trivial::warning;
#endif
			}
		}
#else
		static int GetAndroidLogPriority(int log_level)
		{
			switch (log_level)
			{
			case 0: return ANDROID_LOG_VERBOSE;
			case 1: return ANDROID_LOG_DEBUG;
			case 2: return ANDROID_LOG_INFO;
			case 3: return ANDROID_LOG_WARN;
			case 4: return ANDROID_LOG_ERROR;
			case 5: return ANDROID_LOG_FATAL;
			default:
#if _DEBUG
				return ANDROID_LOG_DEBUG;
#else
				return ANDROID_LOG_WARN;
#endif
			}
		}
#endif // __ANDROID__
	} // namespace (anonymous)

	HSBA_SLICER_LOG_API LoggerSingletone::LoggerSingletone(LoggerSingletone::Private)
	{
		auto current_path = std::filesystem::current_path();
		auto cfg_path = current_path.string() + "/logcfg.ini";
		bool existed = std::filesystem::exists(std::filesystem::path{ cfg_path });
#if _DEBUG
		log_level_ = 1;
#else
		log_level_ = 3;
#endif // _DEBUG
		if (existed)
		{
			auto ptree = Config::from_ini(cfg_path);
			try
			{
#if _DEBUG
				int log_level = ptree.get<int>("log.log_level_debug");
#else
				int log_level = ptree.get<int>("log.log_level");
#endif // _DEBUG
				use_log_file_ = ptree.get<bool>("log.use_log_file");
				log_path_ = current_path.string() + ptree.get<std::string>("log.log_file");
				log_datatime_format_ = ptree.get<std::string>("log_format.log_datatime_format");
			}
			catch (const boost::property_tree::ptree_bad_path&)
			{
				//default config
				log_path_ = current_path.string() + "/log/log.txt";
				use_log_file_ = false;
				log_datatime_format_ = "%Y-%m-%d %H:%M:%S";
			}
		}
		else
		{
			//default config
			log_path_ = current_path.string() + "/log/log.txt";
			use_log_file_ = false;
			log_datatime_format_ = "%Y-%m-%d %H:%M:%S";
		}

#ifndef __ANDROID__
		auto log_level = GetLogLevel(log_level_);

		// configure Boost.Log
		boost::log::formatter log_format = (
			boost::log::expressions::stream <<
			boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", log_datatime_format_) <<
			" <" << boost::log::trivial::severity << "> "
			<< boost::log::expressions::smessage << "\n");
		boost::log::add_common_attributes();
		if (use_log_file_)
		{
			auto file_log = boost::log::add_file_log(
				boost::log::keywords::file_name = log_path_,
				boost::log::keywords::rotation_size = SIZE_50MB,
				boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0),
				boost::log::keywords::open_mode = std::ios::app
			);
			file_log->set_formatter(log_format);
		}

		auto console_log = boost::log::add_console_log();
		console_log->set_formatter(log_format);
		boost::log::core::get()->set_filter(boost::log::trivial::severity >= log_level);
		boost::log::core::get()->add_global_attribute("TimeStamp", boost::log::attributes::local_clock());
#else
		// On Android, use the kernel log. Emit a warning that Android logging is used.
		int log_level = GetAndroidLogPriority(log_level_);
		__android_log_print(ANDROID_LOG_WARN, "HsBaSlicer", "LoggerSingletone initialized: using Android kernel log");
#endif
	}

	HSBA_SLICER_LOG_API bool LoggerSingletone::UseLogFile() const
	{
		std::shared_lock lock{ mutex_ };
#ifdef __ANDROID__
		return true;
#else
		return use_log_file_;
#endif // __ANDROID__
	}

	HSBA_SLICER_LOG_API std::shared_ptr<LoggerSingletone> LoggerSingletone::GetInstance()
	{
		std::call_once(instance_flag_, [&]() {
			instance_ = CreateInstance();
		});
		return instance_;
	}

	HSBA_SLICER_LOG_API void LoggerSingletone::Log(std::string_view message,const int log_lv, const std::source_location& location)
	{
		if (!instance_)
		{
			std::call_once(instance_flag_, [&]() {
				instance_ = std::make_shared<LoggerSingletone>(Private{});
			});
		}
		switch (log_lv)
		{
		case 0:
		{
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_VERBOSE, "HsBaSlicer", "%s%s", GetSourceLocation(location).c_str(), std::string(message).c_str());
#else
			BOOST_LOG_TRIVIAL(trace) << GetSourceLocation(location)<< message;
#endif
		}
			break;
		case 1:
		{
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_DEBUG, "HsBaSlicer", "%s%s", GetSourceLocation(location).c_str(), std::string(message).c_str());
#else
			BOOST_LOG_TRIVIAL(debug) << GetSourceLocation(location) << message;
#endif
		}
			break;
		case 2:
		{
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_INFO, "HsBaSlicer", "%s%s", GetSourceLocation(location).c_str(), std::string(message).c_str());
#else
			BOOST_LOG_TRIVIAL(info) << GetSourceLocation(location) << message;
#endif
		}
			break;
		case 3:
		{
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_WARN, "HsBaSlicer", "%s%s", GetSourceLocation(location).c_str(), std::string(message).c_str());
#else
			BOOST_LOG_TRIVIAL(warning) << GetSourceLocation(location) << message;
#endif
		}
			break;
		case 4:
		{
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_ERROR, "HsBaSlicer", "%s%s", GetSourceLocation(location).c_str(), std::string(message).c_str());
#else
			BOOST_LOG_TRIVIAL(error) << GetSourceLocation(location) << message;
#endif
		}
			break;
		case 5:
		{
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_FATAL, "HsBaSlicer", "%s%s", GetSourceLocation(location).c_str(), std::string(message).c_str());
#else
			BOOST_LOG_TRIVIAL(fatal) << GetSourceLocation(location) << message;
#endif
		}
			break;
		default:
		{
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_INFO, "HsBaSlicer", "%s%s", GetSourceLocation(location).c_str(), std::string(message).c_str());
#else
			BOOST_LOG_TRIVIAL(info) << GetSourceLocation(location) << message;
#endif
		}
			break;
		}
	}

	HSBA_SLICER_LOG_API void LoggerSingletone::LogDebug(std::string_view message, const std::source_location& location)
	{
		if (!instance_)
		{
			std::call_once(instance_flag_, [&]() {
				instance_ = std::make_shared<LoggerSingletone>(Private{});
			});
		}
		Log(message, 1, location);
	}

	HSBA_SLICER_LOG_API void LoggerSingletone::LogInfo(std::string_view message, const std::source_location& location)
	{
		if (!instance_)
		{
			std::call_once(instance_flag_, [&]() {
				instance_ = std::make_shared<LoggerSingletone>(Private{});
			});
		}
		Log(message, 2, location);
	}

	HSBA_SLICER_LOG_API void LoggerSingletone::LogWarning(std::string_view message, const std::source_location& location)
	{
		if (!instance_)
		{
			std::call_once(instance_flag_, [&]() {
				instance_ = std::make_shared<LoggerSingletone>(Private{});
			});
		}
		Log(message, 3, location);
	}

	HSBA_SLICER_LOG_API void LoggerSingletone::LogError(std::string_view message, const std::source_location& location)
	{
		if (!instance_)
		{
			std::call_once(instance_flag_, [&]() {
				instance_ = std::make_shared<LoggerSingletone>(Private{});
			});
		}
		Log(message, 4, location);
	}
	inline namespace LogLiterals
	{
		HSBA_SLICER_LOG_API LogState::LogState(const int log_lv, std::string_view message)
			: log_lv_{ log_lv }, message_{ message }
		{
		}
		HSBA_SLICER_LOG_API LogState::~LogState()
		{
		}
		HSBA_SLICER_LOG_API void LogState::operator()(const std::source_location& location)
		{
			LoggerSingletone::Log(message_, log_lv_, location);
		}
		HSBA_SLICER_LOG_API LogState operator""_log_debug(const char* message, std::size_t size)
		{
			return LogState{ 1, std::string_view{ message, size } };
		}
		HSBA_SLICER_LOG_API LogState operator""_log_info(const char* message, std::size_t size)
		{
			return LogState{ 2, std::string_view{ message, size } };
		}
		HSBA_SLICER_LOG_API LogState operator""_log_warning(const char* message, std::size_t size)
		{
			return LogState{ 3, std::string_view{ message, size } };
		}
		HSBA_SLICER_LOG_API LogState operator""_log_error(const char* message, std::size_t size)
		{
			return LogState{ 4, std::string_view{ message, size } };
		}
	}// namespace HsBa::Slicer::Log::LogLiterals
}// namespace HsBa::Slicer::Log

// 定义LoggerSingletone的静态成员
namespace HsBa::Slicer::Log {
    HSBA_SLICER_LOG_API std::shared_ptr<LoggerSingletone> LoggerSingletone::instance_ = nullptr;
    HSBA_SLICER_LOG_API std::shared_mutex LoggerSingletone::mutex_;
    HSBA_SLICER_LOG_API std::once_flag LoggerSingletone::instance_flag_;
    
    std::shared_ptr<LoggerSingletone> LoggerSingletone::CreateInstance()
    {
        return std::make_shared<LoggerSingletone>(Private{});
    }
}