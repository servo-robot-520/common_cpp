//
// Created by greenhand520 on 2026/5/5.
//

#pragma once

#include <string>
#include <fmt/format.h>

namespace log_interface {

    class LogInterface {

    public:
        virtual ~LogInterface() = default;

        // 日志级别
        enum class Level {
            TRACE,
            DEBUG,
            INFO,
            WARN,
            ERROR,
            CRITICAL,
            NONE,
            N_LEVELS
        };

        virtual void log(Level level, const std::string& file, int line,
                         const std::string& func, const std::string& message) = 0;


        template <typename... Args>
        void log_format(const Level level, const std::string& file, const int line,
                        const std::string& func, fmt::format_string<Args...> fmt, Args&&... args) {
            log(level, file, line, func, fmt::format(fmt, std::forward<Args>(args)...));
        }

    };

}