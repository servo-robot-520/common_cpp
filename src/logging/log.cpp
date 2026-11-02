//
// Created by greenhand520 on 2026/5/5.
//

#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "common_cpp/logging/log.hpp"
#include "common_cpp/logging/filter_sink.hpp"

using namespace logging;

spdlog::level::level_enum logging::str_to_log_level(const std::string& level_str) {
    if (const auto it = LOG_LEVEL_MAP.find(level_str); it != LOG_LEVEL_MAP.end()) {
        return it->second;
    }
    return spdlog::level::level_enum::info;
}

std::string LogConfig::get_logger_name() const {
    return logger_name;
}

void LogConfig::init_from_map(const std::unordered_map<std::string, std::string>& m) {
    log_level = get_or(m, "log.log_level", log_level);
    log_path = get_or(m, "log.log_path", log_path);
    async = get_or(m, "log.async", async);
    logger_name = get_or(m, "log.logger_name", logger_name);
    node_name = get_or(m, "log.node_name", node_name);
    // filter.black 用逗号分隔（如 "start-xxx,inc-yyy"）
    if (const auto it = m.find("log.filter.black"); it != m.end()) {
        std::istringstream ss(it->second);
        std::string token;
        while (std::getline(ss, token, ',')) {
            if (!token.empty())
                filter_black.push_back(token);
        }
    }
}

void Logger::init_file_log_sink(const LogConfig* log_config) {
    const std::filesystem::path log_directory{log_config->log_path};
    if (log_directory.empty()) {
        throw std::invalid_argument("The logging directory must not be empty");
    }

    std::error_code error;
    std::filesystem::create_directories(log_directory, error);
    if (error || !std::filesystem::is_directory(log_directory, error)) {
        const std::string reason = error ? error.message() : "path is not a directory";
        throw std::runtime_error(
            std::format("Unable to create logging directory '{}': {}", log_directory.string(), reason));
    }

    // Generates a separate log file each time it starts up：log_{name}_YYYYMMDD_HHMMSS.log
    const auto now = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_r(&time_t_now, &tm_buf);
    std::ostringstream ts;
    ts << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
    const std::string log_file = (log_directory / std::format("log_{}_{}.log", logger_name_, ts.str())).string();

    // The rcutils bridge can be called from arbitrary executor and worker
    // threads. Use thread-safe sinks for both sync and async loggers.
    const spdlog::sink_ptr file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        log_file, false);
    file_sink->set_level(log_config->log_level);
    const auto file_pattern = fmt::format("[%Y-%m-%d %H:%M:%S.%e] [{}] [%l] [%s:%# %!] %v",
                                          log_config->node_name.empty() ? logger_name_ : log_config->node_name);
    file_sink->set_pattern(file_pattern);
    sinks_.push_back(file_sink);
}

void Logger::init_console_log_sink(const LogConfig* log_config) {
    // spdlog is the sole console writer; it must remain thread-safe even when
    // the logger itself is configured for synchronous writes.
    const spdlog::sink_ptr console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(log_config->log_level);
    const auto console_pattern = fmt::format("[%Y-%m-%d %H:%M:%S.%e] [{}] [%^%l%$] <thread %t> [%s:%# %!] %v",
                                             log_config->node_name.empty() ? logger_name_ : log_config->node_name);
    console_sink->set_pattern(console_pattern);
    sinks_.push_back(console_sink);
}

// Static singleton state. A mutex is used instead of once_flag so a
// configuration attempt after fallback construction can fail loudly.
std::unique_ptr<Logger> Logger::instance_ = nullptr;
std::mutex Logger::instance_mutex_;
Logger::InitializationState Logger::initialization_state_ = Logger::InitializationState::uninitialized;

Logger::Logger() {
    // 默认构造，创建一个最小化的logger，避免未初始化时crash
    logger_name_ = "default";
    const auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);
    sinks_.push_back(console_sink);
    logger_ = std::make_shared<spdlog::logger>(logger_name_, sinks_.begin(), sinks_.end());
    logger_->set_level(spdlog::level::info);
    initialized_ = false; // 默认构造不算真正初始化
}

Logger::Logger(const LogConfig* log_config) {
    if (!log_config) {
        std::cout << "Log config is null" << std::endl;
        throw std::runtime_error("Log config is null");
    }
    logger_name_ = log_config->get_logger_name();

    init_file_log_sink(log_config);
    init_console_log_sink(log_config);

    // 应用黑名单过滤：用 filter_sink 包装真实 sinks
    if (!log_config->filter_black.empty()) {
        std::vector<FilterRule> rules;
        for (const auto& r : log_config->filter_black) {
            if (r.starts_with("start-"))
                rules.push_back({.type = FilterRule::Start, .pattern = r.substr(6)});
            else if (r.starts_with("end-"))
                rules.push_back({.type = FilterRule::End, .pattern = r.substr(4)});
            else if (r.starts_with("inc-"))
                rules.push_back({.type = FilterRule::Include, .pattern = r.substr(4)});
            else if (r.starts_with("equal-"))
                rules.push_back({.type = FilterRule::Equal, .pattern = r.substr(6)});
        }
        if (!rules.empty()) {
            const auto fs = std::make_shared<FilterSink<std::mutex>>(std::move(sinks_));
            for (const auto& [type, pattern] : rules)
                fs->add_rule(type, pattern);
            sinks_.clear();
            sinks_.push_back(fs);
        }
    }

    if (log_config->async) {
        thread_pool_ = std::make_shared<spdlog::details::thread_pool>(1000, 1);
        logger_ = std::make_shared<spdlog::async_logger>(logger_name_, sinks_.begin(), sinks_.end(), thread_pool_);
    }
    else {
        // The bridge retains source-location strings before records reach an
        // async logger. The synchronous logger still uses _mt sinks because
        // rcutils may invoke it concurrently from multiple application threads.
        logger_ = std::make_shared<spdlog::logger>(logger_name_, sinks_.begin(), sinks_.end());
    }

    logger_->set_level(log_config->log_level);
    logger_->flush_on(log_config->log_level);

    register_logger(logger_);
    initialized_ = true;
}

std::shared_ptr<spdlog::logger> Logger::get_logger() const {
    return logger_;
}

Logger::~Logger() {
    // 静态对象析构顺序不确定：spdlog 内部注册表（details::registry）可能
    // 已先于此处被销毁，因此析构函数中不能调用任何 spdlog 注册表 API
    // （spdlog::get / spdlog::drop / spdlog::shutdown 均会访问已释放内存）
    // logger_ 的 shared_ptr 引用计数归零后会自动释放 logger 资源，
    // spdlog 注册表自身的清理由其静态析构函数负责
    if (logger_) {
        logger_->flush();
        logger_.reset();
    }
}

Logger& Logger::get_instance() {
    std::scoped_lock lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::unique_ptr<Logger>(new Logger());
        initialization_state_ = InitializationState::default_initialized;
    }
    return *instance_;
}

void Logger::init_from_config(const LogConfig* config) {
    if (!config) {
        throw std::runtime_error("Log config is null");
    }

    std::scoped_lock lock(instance_mutex_);
    if (initialization_state_ == InitializationState::default_initialized) {
        throw std::logic_error(
            "Logger was accessed before configuration; initialize the logging backend before any Logger::get_instance() call");
    }
    if (initialization_state_ == InitializationState::configured) {
        return;
    }

    instance_ = std::unique_ptr<Logger>(new Logger(config));
    initialization_state_ = InitializationState::configured;
}
