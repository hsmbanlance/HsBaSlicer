#pragma once
#ifndef HSBA_SLICER_APP_CONFIG_HPP
#define HSBA_SLICER_APP_CONFIG_HPP

#include <mutex>
#include <shared_mutex>

namespace HsBa::Slicer
{
/**
 * @brief Application configuration singleton.
 *
 * Provides access to shared application settings such as external tool paths.
 */
class AppConfigSingletone
{
public:
    /**
     * @brief Get the singleton instance.
     *
     * @return AppConfigSingletone& Reference to the singleton.
     */
    static AppConfigSingletone& GetInstance();

    /**
     * @brief Destroy the singleton instance.
     */
    static void DeleteInstance();

    /**
     * @brief Get the configured 7-Zip executable path.
     *
     * @return std::string Path to the 7-Zip executable.
     */
    std::string GetSevenZPath() const;

private:
    static std::shared_mutex mutex_;
    static AppConfigSingletone* instance_;
    std::string sevenZ_path_;
    AppConfigSingletone();
};
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_APP_CONFIG_HPP
