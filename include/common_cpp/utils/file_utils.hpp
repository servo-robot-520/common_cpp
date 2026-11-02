//
// Created by greenhand520 on 2026/7/1.
//

#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace common_cpp::file_utils {

    constexpr std::string CONFIG_DIR_NAME = ".config";

    bool file_exists(const std::string& path);

    bool is_linux();

    /// Only create the last level directory, the parent directory must already exist
    bool create_directory(const std::string& path);

    /// Recursively create all missing intermediate directories. If the parent directory does not exist, it will be created automatically.
    bool create_directories(const std::string& path);

    std::string get_home_directory();

    std::string create_app_data_path(const std::string& app_name);

    std::string get_current_directory();


} // namespace file_utils
