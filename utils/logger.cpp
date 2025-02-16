#include "logger.hpp"

#include <filesystem>

#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include "fileoperator/rw_ptree.hpp"

namespace HsBa::Slicer::Log
{
	namespace
	{
		std::string GetSourceLocation(const std::source_location& location)
		{
			return "[" + std::string{ location.file_name() } + ":" + std::to_string(location.line()) + "] " + location.function_name() + ": ";
		}
	}
	static boost::log::trivial::severity_level GetLogLevel(int log_level)
	{
		switch (log_level)
		{
		case 0:
			return boost::log::trivial::trace;
		case 1:
			return boost::log::trivial::debug;
		case 2:
			return boost::log::trivial::info;
		case 3:
			return boost::log::trivial::warning;
		case 4:
			return boost::log::trivial::error;
		case 5:
			return boost::log::trivial::fatal;
		default:
#if _DEBUG
			return boost::log::trivial::debug;
#else
			return boost::log::trivial::warning;
#endif // _DEBUG
		}
	}


	LoggerSingletone* LoggerSingletone::instance_ = nullptr;

	std::mutex LoggerSingletone::mutex_{};

	LoggerSingletone& LoggerSingletone::GetInstance()
	{
		if (instance_)
		{
			return *instance_;
		}
		else
		{
			std::lock_guard lock{ mutex_ };
			if (!instance_)
			{
				instance_ = new LoggerSingletone();
			}
			return *instance_;
		}
	}

	LoggerSingletone::LoggerSingletone()
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
		auto log_level = GetLogLevel(log_level_);
		//than set log
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
				boost::log::keywords::rotation_size = 50 * 1024 * 1024,
				boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0),
				boost::log::keywords::open_mode = std::ios::app
			);
			file_log->set_formatter(log_format);
		}

		auto console_log = boost::log::add_console_log();
		console_log->set_formatter(log_format);
		boost::log::core::get()->set_filter(boost::log::trivial::severity >= log_level);
		boost::log::core::get()->add_global_attribute("TimeStamp", boost::log::attributes::local_clock());
	}

	void LoggerSingletone::DeleteInstance()
	{
		std::lock_guard lock{ mutex_ };
		if (instance_)
		{
			delete instance_;
			instance_ = nullptr;
		}
	}

	bool LoggerSingletone::UseLogFile() const
	{
		std::lock_guard lock{ mutex_ };
		return use_log_file_;
	}

	void LoggerSingletone::Log(std::string_view message,const int log_lv, const std::source_location& location)
	{
		if (!instance_)
		{
			instance_ = &LoggerSingletone::GetInstance();
		}
		switch (log_lv)
		{
		case 0:
			BOOST_LOG_TRIVIAL(trace) << GetSourceLocation(location)<< message;
			break;
		case 1:
			BOOST_LOG_TRIVIAL(debug) << GetSourceLocation(location) << message;
			break;
		case 2:
			BOOST_LOG_TRIVIAL(info) << GetSourceLocation(location) << message;
			break;
		case 3:
			BOOST_LOG_TRIVIAL(warning) << GetSourceLocation(location) << message;
			break;
		case 4:
			BOOST_LOG_TRIVIAL(error) << GetSourceLocation(location) << message;
			break;
		case 5:
			BOOST_LOG_TRIVIAL(fatal) << GetSourceLocation(location) << message;
			break;
		default:
			BOOST_LOG_TRIVIAL(info) << GetSourceLocation(location) << message;
			break;
		}
	}

	void LoggerSingletone::LogDebug(std::string_view message, const std::source_location& location)
	{
		if (!instance_)
		{
			instance_ = &LoggerSingletone::GetInstance();
		}
		BOOST_LOG_TRIVIAL(debug) << GetSourceLocation(location) << message;
	}

	void LoggerSingletone::LogInfo(std::string_view message, const std::source_location& location)
	{
		if (!instance_)
		{
			instance_ = &LoggerSingletone::GetInstance();
		}
		BOOST_LOG_TRIVIAL(info) << GetSourceLocation(location) << message;
	}

	void LoggerSingletone::LogWarning(std::string_view message, const std::source_location& location)
	{
		if (!instance_)
		{
			instance_ = &LoggerSingletone::GetInstance();
		}
		BOOST_LOG_TRIVIAL(warning) << GetSourceLocation(location) << message;
	}

	void LoggerSingletone::LogError(std::string_view message, const std::source_location& location)
	{
		if (!instance_)
		{
			instance_ = &LoggerSingletone::GetInstance();
		}
		BOOST_LOG_TRIVIAL(error) << GetSourceLocation(location) << message;
	}
	namespace LogLiteral
	{
		LogState::LogState(const int log_lv, std::string_view message)
			: log_lv_{ log_lv }, message_{ message }
		{
		}
		LogState::~LogState()
		{
		}
		void LogState::operator()(const std::source_location& location)
		{
			LoggerSingletone::Log(message_, log_lv_, location);
		}
		LogState operator""_log_debug(const char* message, std::size_t size)
		{
			return LogState{ 1, std::string_view{ message, size } };
		}
		LogState operator""_log_info(const char* message, std::size_t size)
		{
			return LogState{ 2, std::string_view{ message, size } };
		}
		LogState operator""_log_warning(const char* message, std::size_t size)
		{
			return LogState{ 3, std::string_view{ message, size } };
		}
		LogState operator""_log_error(const char* message, std::size_t size)
		{
			return LogState{ 4, std::string_view{ message, size } };
		}
	}// namespace HsBa::Slicer::Log::LogLiteral
}// namespace HsBa::Slicer::Log