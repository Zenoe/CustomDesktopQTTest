#ifndef LOGGER_H_
#define LOGGER_H_

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/msvc_sink.h> // For Visual Studio Output window
#include "ConfigManager.h"
void setup_logging()
{
    // Get full path + rotation settings from config
    std::string logFile     = ConfigManager::instance().get("log.file",
                              std::string("xybox.log"));

    int max_size_mb = ConfigManager::instance().get("log.max_size_mb", 10);
    int max_files   = ConfigManager::instance().get("log.max_files",   5);

    // Convert MB → bytes (spdlog expects size_t)
    size_t max_size = static_cast<size_t>(max_size_mb) * 1024 * 1024;

    // Log to rotating file (automatically creates xybox.log, xybox.1.log, xybox.2.log …)
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFile, max_size, max_files);

    // Log to Visual Studio Output window
    auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

    std::vector<spdlog::sink_ptr> sinks{file_sink, msvc_sink};
    auto logger = std::make_shared<spdlog::logger>("clipboard", sinks.begin(), sinks.end());

    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S][%l][thread %t] %v");
    spdlog::set_level(spdlog::level::info); // change to debug/trace for more details
}
/* void setup_logging() */
/* { */
/*     // Get the full absolute path we stored in the config (includes debug/release subfolder) */
/*     std::string logFile = ConfigManager::instance().get("log.file", */
/*                          std::string("xybox.log"));   // fallback only if config is broken */

/*     // Log to file (using the correct user-writable path, same truncate behaviour you had) */
/*     auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true); */

/*     // Log to Visual Studio Output window */
/*     auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>(); */

/*     std::vector<spdlog::sink_ptr> sinks{file_sink, msvc_sink}; */
/*     auto logger = std::make_shared<spdlog::logger>("clipboard", sinks.begin(), sinks.end()); */

/*     spdlog::set_default_logger(logger); */
/*     spdlog::set_pattern("[%Y-%m-%d %H:%M:%S][%l][thread %t] %v"); */
/*     spdlog::set_level(spdlog::level::info); // change to debug/trace for more details */
/* } */

/* void setup_logging() */
/* { */
/*     // Log to file */
/*     auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("xybox.log", true); */
/*     // Log to Visual Studio Output window */
/*     auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>(); */

/*     std::vector<spdlog::sink_ptr> sinks{file_sink, msvc_sink}; */
/*     auto logger = std::make_shared<spdlog::logger>("clipboard", sinks.begin(), sinks.end()); */
/*     spdlog::set_default_logger(logger); */

/*     spdlog::set_pattern("[%Y-%m-%d %H:%M:%S][%l][thread %t] %v"); */
/*     spdlog::set_level(spdlog::level::info); // change to debug/trace for more details */
/* } */

/* template<typename... Args> */
/* void log_info_w(const std::wstring& fmt, Args&&... args) { */
/*     spdlog::info(string_util::wstring_to_utf8(fmt).c_str(), args...); */
/*     //spdlog::flush(); */
/* } */

// Usage
// Call setup_logging() once at startup, before logging anything.

/* void log_example(int item_id) */
/* { */
/*     spdlog::info("Added clipboard item: {}", item_id); */
/*     spdlog::error("DB error: something went wrong"); */
/* } */




#endif // LOGGER_H_
