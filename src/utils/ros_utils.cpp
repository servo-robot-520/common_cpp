//
// Created by greenhand520 on 2026/7/1.
//

#include "common_cpp/utils/ros_utils.hpp"

#include <filesystem>
#include <fstream>
#include <ament_index_cpp/get_package_share_directory.hpp>

#include <ranges>

#include "common_cpp/utils/common_utils.hpp"

using namespace common_cpp;

std::expected<std::string, std::string> ros_utils::read_package_file_content(const std::string& package, const std::string& file_path) {
    try {
        const auto package_share_dir = ament_index_cpp::get_package_share_directory(package);
        const auto specify_path = std::filesystem::path(package_share_dir) / file_path;

        if (!std::filesystem::exists(specify_path)) {
            return std::unexpected<std::string>(file_path + " does not exist");
        }
        std::ifstream file(specify_path);
        if (!file.is_open()) {
            return std::unexpected<std::string>("Failed to open package file " + specify_path.string());
        }

        std::stringstream srdf_content;
        srdf_content << file.rdbuf();
        file.close();
        return srdf_content.str();
    }
    catch (const std::exception& e) {
        return std::unexpected<std::string>(e.what());
    }
}

bool ros_utils::check_service_available(const rclcpp::Node* node, const std::string& service_name, const uint8_t max_retry,
                                        const uint8_t check_interval_sec) {
    int retry_count = 0;
    while (rclcpp::ok() && retry_count < max_retry) {
        auto service_names = node->get_service_names_and_types();
        for (const auto& name : service_names | std::views::keys) {
            if (name == service_name) {
                return true;
            }
        }
        for (const auto& name : service_names | std::views::keys) {
            if (name.find(service_name) != std::string::npos) {
            }
        }
        retry_count++;
        std::this_thread::sleep_for(std::chrono::seconds(check_interval_sec));
    }
    return false;
}

namespace {

    [[noreturn]] void throw_invalid_argument(const char* const message) {
        throw std::invalid_argument(message);
    }

} // namespace

std::string ros_utils::make_diagnostic_status_name(const std::string_view node_fully_qualified_name,
                                                   const std::string_view hardware_id) {
    if (common_utils::is_empty_or_ascii_whitespace(node_fully_qualified_name)) {
        throw_invalid_argument("node_fully_qualified_name must not be empty or ASCII whitespace");
    }
    if (node_fully_qualified_name == "/") {
        throw_invalid_argument("node_fully_qualified_name must not be '/'");
    }
    if (hardware_id.find('/') != std::string_view::npos) {
        throw_invalid_argument("hardware_id must not contain '/'");
    }

    std::string_view normalized_node_name = node_fully_qualified_name;
    while (normalized_node_name.size() > 1 && normalized_node_name.back() == '/') {
        normalized_node_name.remove_suffix(1);
    }

    std::string result;
    result.append(normalized_node_name);

    if (!hardware_id.empty()) {
        result.reserve(normalized_node_name.size() + 1 + hardware_id.size());
        result.push_back('/');
        result.append(hardware_id);
    }

    return result;
}
