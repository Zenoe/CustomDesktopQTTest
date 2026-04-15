#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

class ConfigManager {
public:
    static ConfigManager& instance();

    void init(const std::string& filePath);

    // Getters
    template<typename T>
    T get(const std::string& key, const T& defaultValue);

    // Setters
    template<typename T>
    void set(const std::string& key, const T& value);

    void saveNow();   // force save
    void shutdown();  // clean shutdown

private:
    ConfigManager();
    ~ConfigManager();

    void load();
    void saveToDisk();

    // background worker
    void workerThread();

    // helpers
    nlohmann::json* getJsonNode(const std::string& key, bool create);

private:
    std::string m_filePath;
    nlohmann::json m_json;

    std::mutex m_mutex;

    std::thread m_worker;
    std::condition_variable m_cv;

    std::atomic<bool> m_dirty{false};
    std::atomic<bool> m_exit{false};
};
