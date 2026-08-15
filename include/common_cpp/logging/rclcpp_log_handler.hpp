//
// Created by greenhand520 on 2026/5/5.
//

#pragma once

#include <iostream>
#include <memory>
#include <rclcpp/utilities.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <rcutils/logging.h>

#include "common_cpp/logging/log.hpp"

namespace logging::ros {

    /// 安装一个进程级全局的 rcutils 输出处理器。每条 RCLCPP_* 日志记录会被同时转发到 ROS 的 /rosout 话题和通用的 spdlog 后端。
    /// spdlog 是唯一的控制台输出者：当此桥接激活时，之前的 rcutils 日志处理器不会被链式调用。
    ///
    /// 这是一个进程生命周期级别的组件。在 rclcpp::init() 之后，从可执行文件的入口点精确调用一次；库代码不得安装它。
    /// 第二次初始化仅在配置完全一致时才被接受。
    /// 在 rclcpp::shutdown() 之前调用 shutdown() 来停止 /rosout 转发并恢复之前的 rcutils 处理器。
    class RclcppLogHandler {
    public:
        /// 支持 rclcpp::Node、rclcpp_lifecycle::LifecycleNode 及兼容的类节点类
        template <typename NodeT>
        static void init_from_node(const std::shared_ptr<NodeT>& node) {
            init_from_node(node.get());
        }

        template <typename NodeT>
        static void init_from_node(NodeT* node) {
            if (node == nullptr) {
                throw std::invalid_argument("Cannot initialize ROS logging from a null node");
            }

            declare_parameter_if_absent(node, "log.log_level", std::string{"info"});
            declare_parameter_if_absent(node, "log.log_path", std::string{"./log"});
            declare_parameter_if_absent(node, "log.async", false);
            declare_parameter_if_absent(node, "log.logger_name", std::string{node->get_name()});
            declare_parameter_if_absent(node, "log.filter.black", std::vector<std::string>{});

            LogConfig config;
            config.log_level = str_to_log_level(node->get_parameter("log.log_level").as_string());
            config.log_path = node->get_parameter("log.log_path").as_string();
            config.async = node->get_parameter("log.async").as_bool();
            config.logger_name = node->get_parameter("log.logger_name").as_string();
            config.node_name = node->get_name();
            config.filter_black = node->get_parameter("log.filter.black").as_string_array();
            init_from_config(config);
        }

        template <typename NodeT>
        static bool try_init_from_node(NodeT* node) {
            try {
                init_from_node(node);
                return true;
            }
            catch (const std::exception& exception) {
                rclcpp::shutdown();
                std::cerr << "Failed to initialize the spdlog backend: "
                          << exception.what() << std::endl;
                return false;
            }
        }

        template <typename NodeT>
        static bool try_init_from_node(const std::shared_ptr<NodeT>& node) {
            return try_init_from_node(node.get());
        }

        static void init_from_map(const std::unordered_map<std::string, std::string>& values);
        static void init_from_config(const LogConfig& log_config);

        /// 在进程关闭前刷新已安装的 spdlog 后端缓冲区。
        static void flush();

        /// 禁用 handler 并恢复之前的 ros 默认的日志处理器，是为了在 ROS 仍处于活动状态时恢复进程全局 rcutils 处理程序。必须在 rclcpp::shutdown() 之前调用；
        /// 注意：这不是一个日志后端实现的切换开关，且调用后不能再次初始化。
        /// 在 ROS 仍处于活动状态时恢复进程全局 rcutils 处理程序。
        static void shutdown();

    private:
        template <typename NodeT, typename ValueT>
        static void declare_parameter_if_absent(NodeT* node, const char* name, ValueT&& default_value) {
            if (!node->has_parameter(name)) {
                node->declare_parameter(name, std::forward<ValueT>(default_value));
            }
        }

        static void log_output_handler(const rcutils_log_location_t* location, int severity,
                                       const char* name, rcutils_time_point_value_t timestamp,
                                       const char* format, va_list* args);
    };

} // namespace common_log
