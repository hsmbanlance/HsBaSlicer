#pragma once
#ifndef HSBA_SLICER_LOGGER_HPP
#define HSBA_SLICER_LOGGER_HPP

#include <shared_mutex>
#include <source_location>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if !defined(__ANDROID__) && !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)
#include <boost/log/core.hpp>
#endif

#if defined(TARGET_OS_IOS) && TARGET_OS_IOS
#include <os/log.h>
#endif

#include "base/singleton.hpp"
#include "export.h"

namespace HsBa::Slicer::Log
{
/**
 * @brief Singleton logger class that provides thread-safe logging functionality.
 *
 * This class implements a singleton pattern for logging with support for multiple log levels,
 * file output, and cross-platform compatibility (iOS, Android, desktop).
 */
class HSBA_SLICER_LOG_API LoggerSingletone
{
public:
    /**
     * @brief Check if logging to file is enabled.
     * @return true if log file is being used, false otherwise.
     */
    bool UseLogFile() const;

    /**
     * @brief Log a message with specified level and source location.
     * @param message The message to log.
     * @param log_lv The log level.
     * @param location Source location information (auto-captured).
     */
    static void Log(std::string_view message, const int log_lv,
                    const std::source_location& location = std::source_location::current());

    /**
     * @brief Log a debug message.
     * @param message The debug message to log.
     * @param location Source location information (auto-captured).
     */
    static void LogDebug(std::string_view message,
                         const std::source_location& location = std::source_location::current());

    /**
     * @brief Log an info message.
     * @param message The info message to log.
     * @param location Source location information (auto-captured).
     */
    static void LogInfo(std::string_view message,
                        const std::source_location& location = std::source_location::current());

    /**
     * @brief Log a warning message.
     * @param message The warning message to log.
     * @param location Source location information (auto-captured).
     */
    static void LogWarning(std::string_view message,
                           const std::source_location& location = std::source_location::current());

    /**
     * @brief Log an error message.
     * @param message The error message to log.
     * @param location Source location information (auto-captured).
     */
    static void LogError(std::string_view message,
                         const std::source_location& location = std::source_location::current());

    /**
     * @brief Get the singleton instance of the logger.
     * @return Shared pointer to the logger instance.
     */
    static std::shared_ptr<LoggerSingletone> GetInstance();

private:
    struct HSBA_SLICER_LOG_API Private
    {
    };

    bool use_log_file_;                ///< Flag indicating whether to use log file
    std::string log_path_;             ///< Path to the log file
    int log_level_;                    ///< Current log level threshold
    std::string log_datatime_format_;  ///< Format string for datetime in logs
#if defined(TARGET_OS_IOS) && TARGET_OS_IOS
    os_log_t log_handle_;  ///< iOS logging handle
#endif
public:
    /**
     * @brief Construct a new LoggerSingletone object.
     * @param Private Tag parameter for controlled construction.
     */
    LoggerSingletone(Private);
    LoggerSingletone(const LoggerSingletone&) = delete;
    LoggerSingletone& operator=(const LoggerSingletone&) = delete;
    LoggerSingletone(LoggerSingletone&&) = delete;
    LoggerSingletone& operator=(LoggerSingletone&&) = delete;
    static std::shared_ptr<LoggerSingletone> instance_;  ///< Singleton instance
    static std::shared_mutex mutex_;                     ///< Mutex for thread safety
    static std::once_flag instance_flag_;                ///< Flag for one-time initialization

    /**
     * @brief Create the singleton instance.
     * @return Shared pointer to the created instance.
     */
    static std::shared_ptr<LoggerSingletone> CreateInstance();
};
inline namespace LogLiterals
{
/**
 * @brief Helper class for fluent logging using user-defined literals.
 *
 * This class enables convenient logging syntax like: "message"_log_info()
 */
class HSBA_SLICER_LOG_API LogState
{
public:
    /**
     * @brief Construct a new LogState object.
     * @param log_lv The log level.
     * @param message The message to log.
     */
    LogState(const int log_lv, std::string_view message);

    /**
     * @brief Destroy the LogState object and flush the log.
     */
    ~LogState();

    LogState(const LogState&) = delete;
    LogState& operator=(const LogState&) = delete;
    LogState(LogState&&) = default;
    LogState& operator=(LogState&&) = default;

    /**
     * @brief Execute the logging operation with source location.
     * @param location Source location information (auto-captured).
     */
    void operator()(const std::source_location& location = std::source_location::current());

private:
    int log_lv_;           ///< Log level
    std::string message_;  ///< Message to be logged
};

/**
 * @brief User-defined literal for debug logging.
 * @param message The message string.
 * @param size The message length.
 * @return LogState object for fluent logging.
 */
HSBA_SLICER_LOG_API LogState operator""_log_debug(const char* message, std::size_t size);

/**
 * @brief User-defined literal for info logging.
 * @param message The message string.
 * @param size The message length.
 * @return LogState object for fluent logging.
 */
HSBA_SLICER_LOG_API LogState operator""_log_info(const char* message, std::size_t size);

/**
 * @brief User-defined literal for warning logging.
 * @param message The message string.
 * @param size The message length.
 * @return LogState object for fluent logging.
 */
HSBA_SLICER_LOG_API LogState operator""_log_warning(const char* message, std::size_t size);

/**
 * @brief User-defined literal for error logging.
 * @param message The message string.
 * @param size The message length.
 * @return LogState object for fluent logging.
 */
HSBA_SLICER_LOG_API LogState operator""_log_error(const char* message, std::size_t size);
}  // namespace LogLiterals
}  // namespace HsBa::Slicer::Log

#endif  // !HSBA_SLICER_LOGGER_HPP
