//
// Created by greenhand520 on 2026/7/18.
//

#include "common_cpp/logging/ros_log_adapter.hpp"

#include <optional>
#include <utility>

#include <rcutils/logging.h>

namespace logging::ros {
    namespace {

        std::optional<int> to_rcutils_severity(const log_interface::LogInterface::Level level) {
            using Level = log_interface::LogInterface::Level;
            switch (level) {
            case Level::TRACE:
            case Level::DEBUG:
                return RCUTILS_LOG_SEVERITY_DEBUG;
            case Level::INFO:
                return RCUTILS_LOG_SEVERITY_INFO;
            case Level::WARN:
                return RCUTILS_LOG_SEVERITY_WARN;
            case Level::ERROR:
                return RCUTILS_LOG_SEVERITY_ERROR;
            case Level::CRITICAL:
                return RCUTILS_LOG_SEVERITY_FATAL;
            case Level::NONE:
            case Level::N_LEVELS:
                return std::nullopt;
            }
            return std::nullopt;
        }

    } // namespace

    RosLogAdapter::RosLogAdapter(std::string logger_name) : logger_name_(std::move(logger_name)) {
    }

    void RosLogAdapter::log(const Level level, const std::string& file, const int line,
                            const std::string& function, const std::string& message) {
        const auto severity = to_rcutils_severity(level);
        if (!severity.has_value()) {
            return;
        }

        const rcutils_log_location_t location{
            .function_name = function.c_str(), .file_name = file.c_str(), .line_number = static_cast<size_t>(line)};
        rcutils_log(&location, *severity, logger_name_.c_str(), "%s", message.c_str());
    }

} // namespace logging::ros
