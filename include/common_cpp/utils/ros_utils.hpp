//
// Created by greenhand520 on 2026/7/1.
//

#pragma once

#include <expected>
#include <string>

#include <rclcpp/node.hpp>

namespace common_cpp::ros_utils {

    /// Read the contents of the specified file under the function package and return
    /// @param package package name
    /// @param file_path File path under package
    /// @return If the file content cannot be read, an error message will be returned.
    std::expected<std::string, std::string> read_package_file_content(const std::string& package, const std::string& file_path);

    bool check_service_available(const rclcpp::Node* node, const std::string& service_name, uint8_t max_retry, uint8_t check_interval_sec);

    /// Constructs a state name that conforms to the diagnostics publisher contract, like:
    /// <node fully-qualified name>/<hardware_id>。
    ///
    /// node_fully_qualified_name must from rclcpp::Node::get_fully_qualified_name()。
    /// This function ignores the extra '/' at the end of node_fully_qualified_name and correctly handles the root path '/',
    /// Ensure that the result has exactly one '/' at the junction of the two parts.
    ///
    /// Any input of an empty string or just ASCII whitespace, as well as a hardware_id containing '/', will throw std::invalid_argument.
    /// Function does not attempt to verify full ROS name syntax; caller should provide RCLCPP
    /// The fully-qualified name that has been normalized.
    [[nodiscard]] std::string make_diagnostic_status_name(std::string_view node_fully_qualified_name, std::string_view hardware_id);

} // namespace common_cpp::ros_utils
