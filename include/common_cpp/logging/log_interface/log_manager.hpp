//
// Created by greenhand520 on 2026/5/5.
//

#pragma once

#include <memory>
#include <mutex>

#include "log_interface.hpp"

namespace log_interface {

    /// LOG_* 宏使用的进程范围门面
    /// 以 shared_ptr 的形式保存和返回，因此并发替换或关闭不会使正在进行的宏调用带有悬空的原始指针。
    class LogManager {
        inline static std::mutex mutex_;
        inline static std::shared_ptr<LogInterface> logger_;

    public:
        static void set(std::shared_ptr<LogInterface> new_logger) {
            std::scoped_lock lock(mutex_);
            logger_ = std::move(new_logger);
        }

        static void clear() {
            std::scoped_lock lock(mutex_);
            logger_.reset();
        }

        [[nodiscard]] static std::shared_ptr<LogInterface> get() {
            std::scoped_lock lock(mutex_);
            return logger_;
        }
    };

} // namespace log_interface
