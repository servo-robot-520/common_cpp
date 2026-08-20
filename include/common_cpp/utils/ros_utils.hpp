//
// Created by greenhand520 on 2026/7/1.
//

#pragma once

#include <expected>
#include <string>

#include <rclcpp/node.hpp>

namespace common_cpp::ros_utils {

    /// 读取功能包下制定文件的内容并返回
    /// @param package 功能包名
    /// @param file_path 功能包下的文件路径
    /// @return 文件内容，读取不到就返回报错内容
    std::expected<std::string, std::string> read_package_file_content(const std::string& package, const std::string& file_path);

    bool check_service_available(const rclcpp::Node* node, const std::string& service_name, uint8_t max_retry, uint8_t check_interval_sec);

    /// 构造符合 diagnostics 发布方契约的状态名称：
    /// <node fully-qualified name>/<hardware_id>。
    ///
    /// node_fully_qualified_name 应来自 rclcpp::Node::get_fully_qualified_name()。
    /// 该函数会忽略 node_fully_qualified_name 末尾多余的 '/'，并正确处理根路径 '/'，
    /// 保证结果在两部分交界处恰好只有一个 '/'。
    ///
    /// 空字符串或仅 ASCII 空白的任一输入，以及包含 '/' 的 hardware_id，均会抛出std::invalid_argument。
    /// 函数不尝试校验完整的 ROS 名称语法；调用者应提供 RCLCPP 已规范化的 fully-qualified name。
    /// 示例输出如下：
    ///   node_fqn	   hardware_id	        结果
    /// /my_ns/talker	 "motor"	/my_ns/talker/motor
    /// /my_ns/talker	    ""	    /my_ns/talker
    /// /talker	            ""	    /talker
    ///   /	                —	    抛异常
    [[nodiscard]] std::string make_diagnostic_status_name(std::string_view node_fully_qualified_name, std::string_view hardware_id);

} // namespace common_cpp::ros_utils
