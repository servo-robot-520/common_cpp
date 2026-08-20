//
// Created by greenhand520 on 2026/7/18.
//

#include "common_cpp/logging/rclcpp_log_handler.hpp"

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include <rcl/logging_rosout.h>
#include <rcutils/logging.h>

#include "common_cpp/logging/log_interface/log_manager.hpp"
#include "common_cpp/logging/ros_log_adapter.hpp"

namespace logging::ros {
    namespace {

        std::mutex initialization_mutex;
        std::optional<LogConfig> installed_config;
        /// 保存被替换掉的前一个 rcutils 输出处理器（用于 restore 时恢复）
        rcutils_logging_output_handler_t preceding_output_handler = nullptr;
        bool bridge_shutdown = false;
        std::atomic_bool bridge_active{false};
        /// rcutils 没有配置中的 "off" 严重级别。
        /// 通过在桥接内部丢弃所有记录，使 "off" 在 /rosout 和 spdlog 之间保持一致。
        /// 这里使用atomic是因为shutdown和log_output_handler可能会并发
        std::atomic_bool bridge_drops_all_records{false};

        /// rcl_logging_rosout_output_handler() 被文档标记为非线程安全。 故意将此互斥锁的范围收窄：spdlog 有自己线程安全的 sink，转发到 ROS 时不持有该锁。
        std::mutex rosout_mutex;
        thread_local bool is_forwarding_to_rosout = false;

        /// spdlog::source_loc 存储的是借来的 char 指针。
        /// 异步 logger 可能在 rcutils_log() 返回之后才格式化记录，因此需要将每个不同的源码位置文本保留到进程终止。
        /// 源码位置通常来自 __FILE__/__func__。
        std::mutex source_text_mutex;
        std::unordered_set<std::string> retained_source_texts;

        /// 比较两个配置是否完全相同。
        [[nodiscard]] bool same_config(const LogConfig& lhs, const LogConfig& rhs) {
            return lhs.log_level == rhs.log_level &&
                lhs.log_path == rhs.log_path &&
                lhs.async == rhs.async &&
                lhs.logger_name == rhs.logger_name &&
                lhs.node_name == rhs.node_name &&
                lhs.filter_black == rhs.filter_black;
        }

        /// 将源码位置字符串永久保留并返回其稳定的 c_str() 指针，防止异步日志乱码
        const char* retain_source_text(const char* value, const char* fallback) {
            std::scoped_lock lock(source_text_mutex);
            const auto [iterator, inserted] = retained_source_texts.emplace(value != nullptr ? value : fallback);
            (void)inserted;
            return iterator->c_str();
        }

        spdlog::level::level_enum rcutils_severity_to_spdlog(const int severity) {
            switch (severity) {
            case RCUTILS_LOG_SEVERITY_DEBUG:
                return spdlog::level::debug;
            case RCUTILS_LOG_SEVERITY_INFO:
                return spdlog::level::info;
            case RCUTILS_LOG_SEVERITY_WARN:
                return spdlog::level::warn;
            case RCUTILS_LOG_SEVERITY_ERROR:
                return spdlog::level::err;
            case RCUTILS_LOG_SEVERITY_FATAL:
                return spdlog::level::critical;
            default:
                return spdlog::level::info;
            }
        }

        int spdlog_level_to_rcutils(const spdlog::level::level_enum level) {
            switch (level) {
            case spdlog::level::trace:
            case spdlog::level::debug:
                return RCUTILS_LOG_SEVERITY_DEBUG;
            case spdlog::level::info:
                return RCUTILS_LOG_SEVERITY_INFO;
            case spdlog::level::warn:
                return RCUTILS_LOG_SEVERITY_WARN;
            case spdlog::level::err:
                return RCUTILS_LOG_SEVERITY_ERROR;
            case spdlog::level::critical:
            case spdlog::level::off:
                return RCUTILS_LOG_SEVERITY_FATAL;
            default:
                return RCUTILS_LOG_SEVERITY_INFO;
            }
        }

        /// 防止 /rosout 转发时出现重入的 RAII 守卫。
        /// 比如一种场景：rcl_logging_rosout_output_handler 内部可能会触发日志（比如序列化消息时遇到错误），这会导致无限递归
        /// thread_local 标志检测当前线程是否已在转发中，如果是就跳过，打断递归链。
        /// 用 thread_local 而不是全局标志，是因为不同线程的调用互不影响，只有同一线程的重入才危险。
        class RosoutReentrancyGuard final {
        public:
            RosoutReentrancyGuard() : was_already_forwarding_(is_forwarding_to_rosout) {
                if (!was_already_forwarding_) {
                    is_forwarding_to_rosout = true;
                }
            }

            ~RosoutReentrancyGuard() {
                if (!was_already_forwarding_) {
                    is_forwarding_to_rosout = false;
                }
            }

            [[nodiscard]] bool can_forward() const {
                return !was_already_forwarding_;
            }

        private:
            bool was_already_forwarding_;
        };

    } // namespace

    void RclcppLogHandler::init_from_map(const std::unordered_map<std::string, std::string>& values) {
        LogConfig config;
        config.init_from_map(values);
        init_from_config(config);
    }

    void RclcppLogHandler::init_from_config(const LogConfig& log_config) {
        std::scoped_lock lock(initialization_mutex);
        // 已经被 shutdown 过的处理器不允许再次初始化。
        if (bridge_shutdown) {
            throw std::logic_error(
                "RclcppLogHandler has already been shut down and cannot be initialized again in this process");
        }
        // 如果已经初始化过，只接受完全相同的配置；不同配置则报错。
        if (installed_config.has_value()) {
            if (!same_config(*installed_config, log_config)) {
                throw std::logic_error(
                    "RclcppLogHandler is process-global and was already initialized with a different configuration");
            }
            return;
        }

        Logger::init_from_config(&log_config);

        const int rcutils_level = spdlog_level_to_rcutils(log_config.log_level);
        if (rcutils_logging_set_logger_level(log_config.logger_name.c_str(), rcutils_level) != RCUTILS_RET_OK) {
            throw std::runtime_error("Failed to set the rcutils logger severity");
        }
        rcutils_logging_set_default_logger_level(rcutils_level);

        preceding_output_handler = rcutils_logging_get_output_handler();
        bridge_drops_all_records.store(log_config.log_level == spdlog::level::off,
                                       std::memory_order_release);
        bridge_active.store(true, std::memory_order_release);
        rcutils_logging_set_output_handler(&RclcppLogHandler::log_output_handler);
        // 安装 ROS 日志适配器，使用 rclcpp 来记录 log_macro.hpp/log_interface 产生的日志
        log_interface::LogManager::set(std::make_shared<RosLogAdapter>(log_config.logger_name));
        installed_config = log_config;
    }

    void RclcppLogHandler::flush() {
        const auto logger = Logger::get_instance().get_logger();
        if (logger) {
            logger->flush();
        }
    }

    void RclcppLogHandler::shutdown() {
        std::scoped_lock lock(initialization_mutex);
        if (!installed_config.has_value() || bridge_shutdown) {
            return;
        }

        // 先刷盘：确保缓冲区里所有日志落盘
        // 此时 bridge_active 仍为 true，spdlog 仍正常工作
        flush();

        // 首先阻止已经在进行中的回调继续访问 ROS /rosout，
        // 然后恢复之前的进程级全局 rcutils 日志处理器，供后续日志使用。
        bridge_active.store(false, std::memory_order_release);
        bridge_drops_all_records.store(false, std::memory_order_release);
        // 等待可能正在进行的 /rosout 转发调用完成，
        // 确保在调用者销毁 rclcpp 之前，非线程安全的 rcl 日志实现已退出。
        {
            std::scoped_lock rosout_lock(rosout_mutex);
            rcutils_logging_set_output_handler(preceding_output_handler);
        }
        log_interface::LogManager::clear();
        bridge_shutdown = true;
    }

    void RclcppLogHandler::log_output_handler(const rcutils_log_location_t* location, const int severity,
                                              const char* name, const rcutils_time_point_value_t timestamp,
                                              const char* format, va_list* args) {
        /// 格式字符串为空、参数为空、或日志级别为 off 时，直接丢弃。
        if (format == nullptr || args == nullptr ||
            bridge_drops_all_records.load(std::memory_order_acquire)) {
            return;
        }

        // 不链式调用前一个处理器：那样会导致 ROS 的默认控制台写入器恢复，
        // 从而使 spdlog 的控制台输出重复。仅转发给专用的 ROS /rosout 处理器，
        // 使用独立的 va_list 副本。
        if (bridge_active.load(std::memory_order_acquire)) {
            if (const RosoutReentrancyGuard reentrancy_guard; reentrancy_guard.can_forward()) {
                std::scoped_lock rosout_lock(rosout_mutex);
                if (bridge_active.load(std::memory_order_acquire)) {
                    va_list rosout_args;
                    va_copy(rosout_args, *args);
                    rcl_logging_rosout_output_handler(location, severity, name, timestamp, format, &rosout_args);
                    va_end(rosout_args);
                }
            }
        }

        /// 用 va_copy 格式化消息字符串（因为 args 只能消费一次，而上面 /rosout 已经消费过了）。
        va_list args_copy;
        va_copy(args_copy, *args);
        const int required_size = std::vsnprintf(nullptr, 0, format, args_copy);
        va_end(args_copy);
        if (required_size < 0) {
            return;
        }

        std::vector<char> buffer(static_cast<size_t>(required_size) + 1U);
        std::vsnprintf(buffer.data(), buffer.size(), format, *args);

        static constexpr char unknown_file[] = "<unknown>";
        static constexpr char unknown_function[] = "<unknown>";
        /// 将源码位置字符串保留到永久存储中，确保 spdlog 的 source_loc 指针在异步格式化时仍然有效。
        const char* const file = retain_source_text(
            location != nullptr ? location->file_name : nullptr, unknown_file);
        const char* const function = retain_source_text(
            location != nullptr ? location->function_name : nullptr, unknown_function);
        const int line = location != nullptr ? static_cast<int>(location->line_number) : 0;

        const auto logger = Logger::get_instance().get_logger();
        if (!logger) {
            return;
        }

        /// 将纳秒级时间戳转换为 spdlog 的时钟类型。
        const auto duration = std::chrono::duration_cast<spdlog::log_clock::duration>(
            std::chrono::nanoseconds(timestamp));
        const spdlog::log_clock::time_point log_time{duration};
        const spdlog::source_loc source{file, line, function};
        const std::string message(buffer.data(), static_cast<size_t>(required_size));
        /// 最终将格式化后的日志交给 spdlog 处理（写控制台 + 按需写文件）。
        logger->log(log_time, source, rcutils_severity_to_spdlog(severity), message);
    }

} // namespace logging::ros
