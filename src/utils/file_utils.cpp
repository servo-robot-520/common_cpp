//
// Created by greenhand520 on 2026/7/1.
//

#include "common_cpp/utils/file_utils.hpp"

inline bool file_utils::file_exists(const std::string& path) {
    return fs::exists(path);
}

bool file_utils::is_linux() {
#ifdef _WIN32
    return false;
#elif _WIN64
    return false;
#elif __linux__
    return true;
#elif __APPLE__
    return false;
#else
    return false;
#endif
}

bool file_utils::create_directory(const std::string& path) {
    try {
        if (const fs::path p = path; fs::create_directory(p)) {
            return true;
        }
        return true; // 存在不算错误
    }
    catch (const fs::filesystem_error& e) {
        return false;
    }
}

bool file_utils::create_directories(const std::string& path) {
    try {
        if (const fs::path p(path); fs::create_directories(p)) {
            return true;
        }
        return true; // 存在不算错误
    }
    catch (const fs::filesystem_error& e) {
        return false;
    }
}

std::string file_utils::get_home_directory() {
    try {
#ifdef _WIN32
        const char* home = std::getenv("USERPROFILE");
        if (home == nullptr) {
            home = std::getenv("HOMEDRIVE");
            if (home != nullptr) {
                std::string homePath = home;
                homePath += std::getenv("HOMEPATH");
                return homePath;
            }
        }
#else
        const char* home = std::getenv("HOME");
#endif

        if (home != nullptr) {
            return fs::path(home).string();
        }

        // 如果环境变量不可用，使用filesystem的备用方案
#ifdef _WIN32
        return fs::path(std::getenv("APPDATA")).parent_path().string();
#else
        return fs::current_path().string();
#endif
    }
    catch (const std::exception& e) {
        return "";
    }
}

std::string file_utils::create_app_data_path(const std::string& app_name) {
    const std::string home_path = get_home_directory();

    // 构建.config路径并创建（如果没有）
    if (!create_directory(CONFIG_DIR_NAME)) {
        return "";
    }

    // 构建app_path路径并创建
    std::string app_data_path = home_path + "/" + CONFIG_DIR_NAME + "/" + app_name;
    if (!create_directories(app_data_path)) {
        return "";
    }

    return app_data_path;
}

std::string file_utils::get_current_directory() {
    return fs::canonical("/proc/self/exe").string();
}
