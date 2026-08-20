//
// Created by greenhand520 on 2026/5/5.
//

#pragma once

#include <string>

#include "common_cpp/logging/log_interface/log_interface.hpp"

namespace logging::ros {

/// 将LOG_* 宏的日志通过 rcutils_log 传送到 ROS /rosout 一次，再传送到公共 spdlog 后端
class RosLogAdapter final : public log_interface::LogInterface {
public:
    explicit RosLogAdapter(std::string logger_name);

    void log(Level level, const std::string& file, int line,
             const std::string& function, const std::string& message) override;

private:
    std::string logger_name_;
};

}  // namespace common_log
